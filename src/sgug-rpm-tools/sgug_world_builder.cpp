#include "helpers.hpp"
#include "specfile.hpp"
#include "installedrpm.hpp"
#include "standalonerpm.hpp"
#include "dependencyset.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <optional>

#include <rpm/header.h>
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

static char * inputdir = NULL;
static char * outputdir = NULL;
static char * gitrootdir = NULL;

static struct poptOption optionsTable[] = {
  {
    NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
    "Common options for all rpm modes and executables",
    NULL },
  {
    "inputdir",
    'i',
    POPT_ARG_STRING,
    &inputdir,
    0,
    "Input directory where SRPM packages may be found",
    NULL
  },
  {
    "outputdir",
    'o',
    POPT_ARG_STRING,
    &outputdir,
    0,
    "Output directory where SRPM and RPM files will be placed",
    NULL
  },
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

optional<string> find_srpm_for_package( bool verbose,
					string sgug_rse_srpm_archive_root,
					string srpm_name )
{
  path candidate_path = std::filesystem::path(sgug_rse_srpm_archive_root);
  if( verbose ) {
    cout << "# Looking for " << srpm_name << " under " << candidate_path << endl;
  }
  if( fs::exists(candidate_path) && fs::is_directory(candidate_path)) {
    vector<path> candidates;
    for( const auto & entry : fs::directory_iterator(candidate_path)) {
      path entry_path = entry.path();
      string entry_filename = entry_path.filename();
      //      if( verbose ) {
      //	cout << "# Looking for " << srpm_name << " at " << entry_filename <<
      //	  endl;
      //      }
      if((!fs::is_directory(entry_path)) &&
	 sgug_rpm::str_starts_with(entry_filename,srpm_name) &&
	 sgug_rpm::str_ends_with(entry_filename, ".src.rpm") ) {
	candidates.push_back(entry_path);
	if( verbose ) {
	  cout << "# Found candidate: " << entry_filename << endl;
	}
      }

    }
    if(candidates.size() > 0) {
      for( const path & cp : candidates ) {
	sgug_rpm::standalonerpm sarpm;
	string canonical_filename = fs::canonical(cp);
	if( sgug_rpm::read_standalonerpm( verbose, canonical_filename,
					  sarpm ) ) {
	  if( sarpm.get_name() == srpm_name ) {
	    return {cp.filename()};
	  }
	  else if(verbose) {
	    cout << "# Examined " << canonical_filename <<
	      " but failed match of package name" << endl;
	  }
	}
	else if(verbose) {
	  cerr << "# Couldn't open " << canonical_filename << endl;
	}
      }
    }
      
    return {};
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
  if( inputdir == NULL || outputdir == NULL || gitrootdir == NULL ) {
    cerr << "inputdir, outputdir and gitrootdir must be passed" << endl;
    exit(EXIT_FAILURE);
  }
  path inputdir_p = {inputdir};
  path outputdir_p = {outputdir};
  path gitrootdir_p = {gitrootdir};

  bool verbose = popt_context.verbose;

  sgug_rpm::progress_printer pprinter;

  path inputsrpm_p = inputdir_p;
  path buildprogress_p = outputdir_p / "PROGRESS";
  path outputsrpm_p = outputdir_p / "SRPMS";
  path outputrpm_p = outputdir_p / "RPMS";

  /*
  string sgug_rse_git_root = "/usr/people/dan/Sources/GitClones/sgug-rse.git";
  string build_progress_dir = "/usr/people/dan/Temp/build0.0.6round2/PROGRESS";
  string sgug_rse_srpm_archive_root = "/usr/people/dan/Temp/build0.0.6round1/SRPMS";
  string sgug_rse_srpm_output_root = "/usr/people/dan/Temp/build0.0.6round2/SRPMS";
  string sgug_rse_rpm_output_root = "/usr/people/dan/Temp/build0.0.6round2/RPMS";
  */

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
    exit(EXIT_FAILURE);
  }

  vector<sgug_rpm::specfile> specs_to_rebuild = valid_specfiles;

  cout << "# Checking availability of SRPMs for packages..." << endl;
  unordered_map<string,string> package_to_srpm_map;

  for( const sgug_rpm::specfile & specfile : valid_specfiles ) {
    const string & srpm_name = specfile.get_name();
    cout << "# Looking for srpm " << srpm_name << endl;
    optional<string> found_srpm_opt =
      find_srpm_for_package( verbose, inputsrpm_p, srpm_name );
    if( !found_srpm_opt ) {
      cerr << "Unable to find SRPM for " << srpm_name << endl;
      exit(EXIT_FAILURE);
    }
    package_to_srpm_map[srpm_name] = *found_srpm_opt;
  }

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
  worldrebuilderfile << "echo 'This script is SUPER DESTRUCTIVE.'" << endl;
  worldrebuilderfile << "echo 'So you must edit it which confirms you'" << endl;
  worldrebuilderfile << "echo 'agree with what it will do.'" << endl;
  worldrebuilderfile << "exit 1" << endl;
  worldrebuilderfile << "# Some useful variables" << endl;
  worldrebuilderfile << "build_progress_dir=" << buildprogress_p << endl;
  worldrebuilderfile << "sgug_rse_srpm_archive_root=" << inputsrpm_p << endl;
  worldrebuilderfile << "sgug_rse_git_root=" << gitrootdir_p << endl;
  worldrebuilderfile << "sgug_rse_srpm_output_root=" << outputsrpm_p << endl;
  worldrebuilderfile << "sgug_rse_rpm_output_root=" << outputrpm_p << endl;

  worldrebuilderfile << "ORIG_WD=`pwd`" << endl;
  worldrebuilderfile << "cleanUpDirs () {" << endl;
  worldrebuilderfile << "    rm -rf ~/rpmbuild" << endl;
  worldrebuilderfile << "    mkdir -p ~/rpmbuild/BUILD" << endl;
  worldrebuilderfile << "    mkdir -p ~/rpmbuild/BUILDROOT" << endl;
  worldrebuilderfile << "    mkdir -p ~/rpmbuild/RPMS" << endl;
  worldrebuilderfile << "    mkdir -p ~/rpmbuild/SOURCES" << endl;
  worldrebuilderfile << "    mkdir -p ~/rpmbuild/SPECS" << endl;
  worldrebuilderfile << "    mkdir -p ~/rpmbuild/SRPMS" << endl;
  worldrebuilderfile << "    mkdir -p $build_progress_dir" << endl;
  worldrebuilderfile << "    mkdir -p $sgug_rse_srpm_output_root" << endl;
  worldrebuilderfile << "    mkdir -p $sgug_rse_rpm_output_root/noarch" << endl;
  worldrebuilderfile << "    mkdir -p $sgug_rse_rpm_output_root/mips" << endl;
  worldrebuilderfile << "}" << endl;
  worldrebuilderfile << "installSrpm () {" << endl;
  worldrebuilderfile << "    packageSrpmfile=$1" << endl;
  worldrebuilderfile << "    rpm -ivh $packageSrpmfile" << endl;
  worldrebuilderfile << "}" << endl;
  worldrebuilderfile << "copySgugGitPackage () {" << endl;
  worldrebuilderfile << "    sgugGitPackageRoot=$1" << endl;
  worldrebuilderfile << "    cp -r $sgugGitPackageRoot/* ~/rpmbuild/" << endl;
  worldrebuilderfile << "}" << endl;
  worldrebuilderfile << "rpmbuildPackage () {" << endl;
  worldrebuilderfile << "    packageName=$1" << endl;
  worldrebuilderfile << "    cd ~/rpmbuild/SPECS" << endl;
  worldrebuilderfile << "    rpmbuild -ba \"$packageName.spec\" --nocheck" << endl;
  worldrebuilderfile << "    rpmrc=$?" << endl;
  worldrebuilderfile << "    cd $ORIG_WD" << endl;
  worldrebuilderfile << "    return $rpmrc" << endl;
  worldrebuilderfile << "}" << endl;
  worldrebuilderfile << "archiveBuiltArtefacts () {" << endl;
  worldrebuilderfile << "    mv ~/rpmbuild/SRPMS/* $sgug_rse_srpm_output_root/" << endl;
  worldrebuilderfile << "    if [[ -e ~/rpmbuild/RPMS/noarch ]]; then" << endl;
  worldrebuilderfile << "	mv ~/rpmbuild/RPMS/noarch/* $sgug_rse_rpm_output_root/noarch/" << endl;
  worldrebuilderfile << "    fi" << endl;
  worldrebuilderfile << "    if [[ -e ~/rpmbuild/RPMS/mips ]]; then" << endl;
  worldrebuilderfile << "	mv ~/rpmbuild/RPMS/mips/* $sgug_rse_rpm_output_root/mips/" << endl;
  worldrebuilderfile << "    fi" << endl;
  worldrebuilderfile << "}" << endl;
  worldrebuilderfile << "doPackageBuild () {" << endl;
  worldrebuilderfile << "    packageName=$1" << endl;
  worldrebuilderfile << "    packageSrpm=$2" << endl;
  worldrebuilderfile << "    startedFilename=\"$build_progress_dir/$packageName.started\"" << endl;
  worldrebuilderfile << "    failedFilename=\"$build_progress_dir/$packageName.failed\"" << endl;
  worldrebuilderfile << "    successFilename=\"$build_progress_dir/$packageName.success\"" << endl;
  worldrebuilderfile << "    if [[ -e $startedFilename ]]; then" << endl;
  worldrebuilderfile << "	echo \"$packageName was previously started. Skipping.\"" << endl;
  worldrebuilderfile << "    else" << endl;
  worldrebuilderfile << "	cleanUpDirs" << endl;
  worldrebuilderfile << "	touch $startedFilename" << endl;
  worldrebuilderfile << "	installSrpm \"$sgug_rse_srpm_archive_root/$packageSrpm\"" << endl;
  worldrebuilderfile << "	copySgugGitPackage \"$sgug_rse_git_root/packages/$packageName\"" << endl;
  worldrebuilderfile << "	rpmbuildPackage \"$packageName\"" << endl;
  worldrebuilderfile << "	packageBuildStatus=$?" << endl;
  worldrebuilderfile << "	if [[ $packageBuildStatus -ne 0 ]]; then" << endl;
  worldrebuilderfile << "	    touch $failedFilename" << endl;
  worldrebuilderfile << "	else" << endl;
  worldrebuilderfile << "	    archiveBuiltArtefacts" << endl;
  worldrebuilderfile << "	    touch $successFilename" << endl;
  worldrebuilderfile << "	fi" << endl;
  worldrebuilderfile << "    fi" << endl;
  worldrebuilderfile << "}" << endl;
  worldrebuilderfile << "# The package list..." << endl;

  for( const sgug_rpm::specfile & spec : specs_to_rebuild ) {
    const string & name = spec.get_name();
    const string & srpm = package_to_srpm_map[name];
    worldrebuilderfile << "doPackageBuild '" << name << "' '" <<
      srpm << "'" << endl;
    //    worldrebuilderfile << "touch ~/rpmbuild/PROGRESS/" << name << ".start" <<
    //      endl;
    //    worldrebuilderfile << "rpmbuild -ba " << name << ".spec --nocheck" <<
    //      endl;
    //    worldrebuilderfile << "if [[ $? -ne 0 ]]; then" << endl;
    //    worldrebuilderfile << "  touch ~/rpmbuild/PROGRESS/" << name <<
    //      ".failed" << endl;
    //    worldrebuilderfile << "fi" << endl;
    //    worldrebuilderfile << "touch ~/rpmbuild/PROGRESS/" << name << ".done" <<
    //      endl;
  }

  worldrebuilderfile.close();

  return 0;
}
