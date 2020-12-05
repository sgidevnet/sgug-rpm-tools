#include "helpers.hpp"
#include "specfile.hpp"
#include "installedrpm.hpp"
#include "dependencyset.hpp"
#include "sgug_dep_engine.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <optional>

#include <rpm/rpmcli.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmts.h>
#include <rpm/rpmarchive.h>
#include <rpm/rpmlog.h>

// C++ structures/algorithms
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>

using std::cerr;
using std::cout;
using std::endl;
using std::optional;
using std::ofstream;
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::vector;

using std::filesystem::path;

namespace fs = std::filesystem;

static char * gitrootdir = NULL;

static struct poptOption optionsTable[] = {
  {
    NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
    "Common options for all rpm modes and executables",
    NULL },
  {
    "gitroot",
    'g',
    POPT_ARG_STRING,
    &gitrootdir,
    0,
    "RSE git repository directory containing releasepackages.lst",
    NULL
  },
  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

optional<string> calculate_expected_specfile_path( string sgug_rse_git_root,
					 string package_name ) {
  path srgr_path = std::filesystem::path(sgug_rse_git_root);
  path package_spec_path = srgr_path / "packages" / package_name / "SPECS" / (package_name + ".spec");
  if( fs::exists(package_spec_path) ) {
    return {fs::canonical(package_spec_path)};
  }
  else {
    return {};
  }
}

int main(int argc, char**argv)
{
  vector<sgug_rpm::specfile> valid_specfiles;
  vector<string> failed_specfiles;

  sgug_rpm::poptcontext_h popt_context( argc, argv, optionsTable );
  rpmlogSetMask(RPMLOG_ERR);

  if( popt_context.context == NULL ) {
    exit(EXIT_FAILURE);
  }

  // Check we have outputdir and gitrootdir
  if( gitrootdir == NULL ) {
    cerr << "gitrootdir must be passed" << endl;
    exit(EXIT_FAILURE);
  }
  path gitrootdir_p = {gitrootdir};
  vector<string> spec_filenames;

  // Capture any explicit minimum rpms
  const char ** fnp;
  for( fnp = poptGetArgs(popt_context.context); fnp && *fnp; ++fnp ) {
    spec_filenames.push_back(*fnp);    
  }

  bool verbose = popt_context.verbose;

  sgug_rpm::progress_printer pprinter;

  cout << "# Reading spec files..." << endl;

  std::ifstream input( fs::canonical(gitrootdir_p / "releasepackages.lst") );

  vector<string> names_in;
  string curline;
  for( string line; std::getline(input, line); ) {
    // Skip comments + empty lines
    if( line.length() == 0 || line[0] == '#' ) {
      continue;
    }
    // For each package, check we have
    // a) a specfile
    // b) an SRPM we can install that matches
    string package_name = line;
    names_in.push_back(package_name);
  }
  input.close();

  for( string & package_name : names_in ) {
    optional<string> expected_specfile_path_opt =
      calculate_expected_specfile_path( gitrootdir_p,
					package_name );
    if( !expected_specfile_path_opt ) {
      cerr << "Missing spec for " << package_name << endl;
      cerr << "Looked under " << gitrootdir_p << "/packages/" <<
	package_name << "/SPECS/" << package_name << ".spec" << endl;
      exit(EXIT_FAILURE);
    }
    string expected_specfile_path = *expected_specfile_path_opt;
    sgug_rpm::specfile specfile;
    if( verbose ) {
      cout << "# Checking for spec at " << expected_specfile_path << endl;
    }
    rpmSpecFlags flags = (RPMSPEC_FORCE);
    if( sgug_rpm::read_specfile( expected_specfile_path,
				 flags,
				 specfile,
				 pprinter ) ) {
      valid_specfiles.emplace_back( specfile );
    }
    else {
      failed_specfiles.push_back( package_name );
    }
    // Quick hack while testing, only do the first one
    //break;
  }

  size_t num_specs = valid_specfiles.size();
  if( num_specs == 0 ) {
    cerr << "No valid spec files found." << endl;
    exit(EXIT_FAILURE);
  }

  size_t num_packages=0;
  for( const sgug_rpm::specfile & specfile : valid_specfiles ) {
    num_packages += specfile.get_packages().size();
  }

  cout << "# Found " << num_specs <<
    " .spec file(s) = " << num_packages << " rpms" << endl;
  size_t num_failed_specs = failed_specfiles.size();
  if( num_failed_specs > 0 ) {
    cout << "# There were " << num_failed_specs <<
      " .spec file(s) that could not be parsed:" << endl;
    for( const string & failed_fn : failed_specfiles ) {
      cout<< "#     " << failed_fn << endl;
    }
  }

  // Now we work out for each of the rpms
  // (a) If such an RPM is installed
  // (b) what the dependencies are for those that are

  vector<sgug_rpm::installedrpm> rpms_to_resolve;
  vector<string> uninstalled_rpms;

  cout << "# Checking for installed packages and dependencies..." << endl;

  for( const sgug_rpm::specfile & specfile: valid_specfiles ) {
    //    cout << "# Walking spec " << specfile.get_name() << endl;
    size_t num_valid_rpms = 0;
    for( const string & pkg : specfile.get_packages() ) {
      sgug_rpm::installedrpm foundrpm;
      bool valid_rpm = sgug_rpm::read_installedrpm( verbose, pkg, foundrpm );
      if( valid_rpm ) {
	rpms_to_resolve.emplace_back(foundrpm);
      }
      else {
	uninstalled_rpms.push_back(pkg);
      }
    }
    pprinter.accept_progress();
  }
  pprinter.reset();

  size_t num_installed_rpms = rpms_to_resolve.size();
  cout << "# Found " << num_installed_rpms <<
    " installed rpm(s)" << endl;
  size_t num_uninstalled_rpms = num_packages - num_installed_rpms;
  if( num_uninstalled_rpms > 0 ) {
    cout << "# There were " << num_uninstalled_rpms <<
      " rpm(s) that aren't installed:" << endl;
    for( const string & uninstalled_rpm : uninstalled_rpms ) {
      cout<< "#     " << uninstalled_rpm << endl;
    }
  }

  cout << "# Computing minimal set..." << endl;

  unordered_set<string> special_packages;
  special_packages.emplace("rpm");
  special_packages.emplace("sudo");
  special_packages.emplace("vim-minimal");
  special_packages.emplace("tar");
  special_packages.emplace("bzip2");
  special_packages.emplace("gzip");
  special_packages.emplace("xz");
  special_packages.emplace("unzip");
  /*special_packages.emplace("sgugshell");*/
  special_packages.emplace("dnf-data");
  special_packages.emplace("microdnf");
  special_packages.emplace("tdnf");
  /*special_packages.emplace("git-all");*/
  /*special_packages.emplace("sgug-getopt");*/

  vector<string> missing_deps;

  vector<sgug_rpm::resolvedrpm> resolved_rpms =
    sgug_rpm::flatten_sort_packages( rpms_to_resolve, 
				     [&](const string & pkg_name) -> bool {
				       if(special_packages.find(pkg_name) !=
					  special_packages.end()) {
					 return true;
				       }
				       else {
					 return false;
				       }
				     },
				     missing_deps,
				     pprinter );

  if( verbose ) {
    uint32_t count = 0;
    for( sgug_rpm::resolvedrpm & rrpm : resolved_rpms ) {
      cout << "STATE: " << rrpm.get_package().get_name() << ":" <<
	rrpm.get_sequence_no() << " " << count << " special: " <<
	rrpm.get_special() <<endl;
      count++;
    }

    count = 0;
    for( sgug_rpm::resolvedrpm & rrpm : resolved_rpms ) {
      if( rrpm.get_special() ) {
	cout << "SPECIAL: " << rrpm.get_package().get_name() << ":" <<
	  rrpm.get_sequence_no() << " " << count << " special: " <<
	  rrpm.get_special() << " rpmfile: " <<
	  rrpm.get_package().get_rpmfile() << endl;
	count++;
      }
    }
  }

  cout << "Writing output files..." <<
    endl;

  ofstream missingdepsfile;
  missingdepsfile.open("missingdeps.txt");
  for( auto & md : missing_deps ) {
    missingdepsfile << md << endl;
  }

  ofstream leaveinstalledfile;
  leaveinstalledfile.open("leaveinstalled.txt");

  ofstream removeexistingfile;
  removeexistingfile.open("removeexisting.sh");
  removeexistingfile << "#!/usr/sgug/bin/bash" << endl;
  removeexistingfile << "# This script should be run under sudo!" << endl;
  removeexistingfile << "# (and may not work properly due to perl circular " \
    "dependencies)" << endl;

  // Leave installed we have in the order from least-deps to most-deps
  for( sgug_rpm::resolvedrpm & rrpm : resolved_rpms ) {
    if( rrpm.get_special() ) {
      leaveinstalledfile << rrpm.get_package().get_name() << " " <<
	rrpm.get_package().get_rpmfile() << endl;
    }
  }

  // While for remove existing we reverse that to easier remove
  // First we print a bunch of comments with the package names on a line
  bool have_rpms_to_remove=false;
  std::reverse(resolved_rpms.begin(), resolved_rpms.end());
  for( sgug_rpm::resolvedrpm & rrpm : resolved_rpms ) {
    if( !rrpm.get_special() ) {
      have_rpms_to_remove=true;
      removeexistingfile << "# Should remove " <<
	rrpm.get_package().get_name() << " " <<
	rrpm.get_package().get_rpmfile() << endl;
    }
  }
  // Now we'll print a big fat long line with all the removes together
  // (It's quicker/more correct to get rpm to do it all at once.)
  if( have_rpms_to_remove ) {
    removeexistingfile << "rpm -evh";
  }
  for( sgug_rpm::resolvedrpm & rrpm : resolved_rpms ) {
    if( !rrpm.get_special() ) {
      removeexistingfile << " " << rrpm.get_package().get_name();
    }
  }
  if( have_rpms_to_remove ) {
    removeexistingfile << endl;
  }

  removeexistingfile.close();
  leaveinstalledfile.close();
  missingdepsfile.close();

  return 0;
}
