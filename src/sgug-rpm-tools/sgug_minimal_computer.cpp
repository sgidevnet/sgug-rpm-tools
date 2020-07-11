#include "helpers.hpp"
#include "specfile.hpp"
#include "installedrpm.hpp"
#include "dependencyset.hpp"
#include "sgug_dep_engine.hpp"

#include <iostream>
#include <fstream>

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

static struct poptOption optionsTable[] = {
  {
    NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
    "Common options for all rpm modes and executables",
    NULL },
  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int main(int argc, char**argv)
{
  vector<sgug_rpm::specfile> valid_specfiles;
  vector<string> failed_specfiles;

  sgug_rpm::poptcontext_h popt_context( argc, argv, optionsTable );
  rpmlogSetMask(RPMLOG_ERR);

  if( popt_context.context == NULL ) {
    exit(EXIT_FAILURE);
  }

  vector<string> spec_filenames;

  // Capture any explicit minimum rpms
  const char ** fnp;
  for( fnp = poptGetArgs(popt_context.context); fnp && *fnp; ++fnp ) {
    spec_filenames.push_back(*fnp);    
  }

  bool verbose = popt_context.verbose;

  sgug_rpm::progress_printer pprinter;

  cout << "# Reading spec files..." << endl;

  rpmSpecFlags flags;

  if( spec_filenames.size() > 0 ) {
    for( const string & spec_file : spec_filenames ) {
      sgug_rpm::specfile dest;
      // Must reset rpm macros every time to be sure
      // no global are remembered
      popt_context.reset_rpm_macros();
      if( sgug_rpm::read_specfile( spec_file, flags, dest, pprinter ) ) {
	valid_specfiles.emplace_back(dest);
      }
      else {
	failed_specfiles.push_back(spec_file);
      }
      pprinter.accept_progress();
    }
    pprinter.reset();
  }
  else {
    sgug_rpm::read_rpmbuild_specfiles( popt_context,
				       flags,
				       valid_specfiles,
				       failed_specfiles,
				       pprinter );
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
  special_packages.emplace("git-all");
  special_packages.emplace("tar");
  special_packages.emplace("bzip2");
  special_packages.emplace("gzip");
  special_packages.emplace("xz");
  special_packages.emplace("unzip");
  special_packages.emplace("sgug-getopt");

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

  cout << "Writing leaveinstalled.txt and removeexisting.sh results." <<
    endl;

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

  return 0;
}
