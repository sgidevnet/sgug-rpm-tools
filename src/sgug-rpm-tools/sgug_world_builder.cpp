#include "helpers.hpp"
#include "specfile.hpp"
#include "installedrpm.hpp"
#include "dependencyset.hpp"

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

using std::string;
using std::cout;
using std::cerr;
using std::cin;
using std::endl;
using std::ofstream;
using std::vector;
using std::unordered_map;
using std::unordered_set;

using std::optional;

using std::filesystem::path;

namespace fs = std::filesystem;

static struct poptOption optionsTable[] = {
  {
    NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
    "Common options for all rpm modes and executables",
    NULL },
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

  bool verbose = popt_context.verbose;

  sgug_rpm::progress_printer pprinter;

  string sgug_rse_git_root = "/usr/people/dan/Sources/GitClones/sgug-rse.git";
  string sgug_rse_srpm_archive_root = "/usr/people/dan/Temp/srpmfetches0.0.6";

  cout << "# Reading spec files..." << endl;

  string curline;
  for( string line; std::getline(cin, line); ) {
    // Skip comments + empty lines
    if( line.length() == 0 || line[0] == '#' ) {
      continue;
    }
    // For each package, check we have
    // a) a specfile
    // b) an SRPM we can install that matches
    string package_name = line;
    optional<string> expected_specfile_path_opt =
      calculate_expected_specfile_path( sgug_rse_git_root,
					package_name );
    if( !expected_specfile_path_opt ) {
      cerr << "Missing spec for " << package_name << endl;
      cerr << "Looked under " << sgug_rse_git_root << "/packages/" <<
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

  exit(1);
  // GOT HERE

  // Now we work out for each of the rpms
  // (a) If such an RPM is installed

  vector<sgug_rpm::specfile> specs_to_rebuild;

  cout << "# Checking for installed packages and dependencies..." << endl;

  for( const sgug_rpm::specfile & specfile: valid_specfiles ) {
    //cout << "# Walking spec " << specfile.get_name() << endl;
    size_t num_valid_rpms = 0;
    bool found_installed_package = false;
    for( const string & pkg : specfile.get_packages() ) {
      sgug_rpm::installedrpm foundrpm;
      bool valid_rpm = sgug_rpm::read_installedrpm( verbose, pkg, foundrpm );
      if( valid_rpm ) {
	found_installed_package = true;
      }
    }
    pprinter.accept_progress();
    if( found_installed_package ) {
      specs_to_rebuild.emplace_back(specfile);
    }
  }
  pprinter.reset();

  cout << "# Writing worldrebuilder.sh..." << endl;

  // Get everything in nice a->z order
  std::sort(specs_to_rebuild.begin(), specs_to_rebuild.end(),
	    [](const sgug_rpm::specfile & a, const sgug_rpm::specfile & b ) -> bool {
	      return a.get_name() < b.get_name();
	    });

  ofstream worldrebuilderfile;
  worldrebuilderfile.open("worldrebuilder.sh");
  worldrebuilderfile << "#!/usr/sgug/bin/bash" << endl;
  worldrebuilderfile << "# This script should be run as your user!" << endl;
  worldrebuilderfile << "mkdir -p ~/rpmbuild/PROGRESS" << endl;

  for( const sgug_rpm::specfile & spec : specs_to_rebuild ) {
    const string & name = spec.get_name();
    worldrebuilderfile << "# Must rebuild: " << name << endl;
    worldrebuilderfile << "touch ~/rpmbuild/PROGRESS/" << name << ".start" <<
      endl;
    worldrebuilderfile << "rpmbuild -ba " << name << ".spec --nocheck" <<
      endl;
    worldrebuilderfile << "if [[ $? -ne 0 ]]; then" << endl;
    worldrebuilderfile << "  touch ~/rpmbuild/PROGRESS/" << name <<
      ".failed" << endl;
    worldrebuilderfile << "fi" << endl;
    worldrebuilderfile << "touch ~/rpmbuild/PROGRESS/" << name << ".done" <<
      endl;
  }

  worldrebuilderfile.close();

  return 0;
}
