#include "helpers.hpp"
#include "specfile.hpp"
#include "installedrpm.hpp"
#include "standalonerpm.hpp"
#include "dependencyset.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
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
#include <functional>

using std::string;
using std::cout;
using std::cerr;
using std::cin;
using std::endl;
using std::ofstream;
using std::stringstream;
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
  sgug_rpm::poptcontext_h popt_context( argc, argv, optionsTable );
  rpmlogSetMask(RPMLOG_ERR);

  if( popt_context.context == NULL ) {
    exit(EXIT_FAILURE);
  }

  // Process the package passed on the command line
  vector<string> names_in;
  const char * extra_arg;
  while( (extra_arg = poptGetArg(popt_context.context)) != NULL ) {
    names_in.push_back(extra_arg);
  }

  if( names_in.size() == 0 ) {
    cerr << "No packages specified. Exiting." << endl;
    exit(EXIT_FAILURE);
  }

  bool verbose = popt_context.verbose;

  sgug_rpm::progress_printer pprinter;

  sgug_rpm::specfile specfile;
  for( const string & one_package : names_in ) {
    if( verbose ) {
      cout << "# Opening spec " << one_package << endl;
    }
    rpmSpecFlags flags = (RPMSPEC_FORCE);
    if( !sgug_rpm::read_specfile( one_package,
				  flags,
				  specfile,
				  pprinter ) ) {
      cerr << "Unable to read specfile " << one_package << endl;
      exit(EXIT_FAILURE);
    }
  }
  
  cout << "PackageName: " << specfile.get_name() << endl;
  cout << "PackagePath: " << specfile.get_filepath() << endl;
  const vector<string> & packages = specfile.get_packages();
  for( const string & sub_package : packages ) {
    cout << "OutputRPM: " << sub_package << endl;
  }
  const unordered_map<string,vector<string>> & build_deps =
    specfile.get_build_deps();
  for( auto & entry : build_deps ) {
    if( verbose ) {
      cout << "# Examining entry " << entry.first << endl;
    }
    const vector<string> & entry_deps = entry.second;
    for( const string & builddep_package : entry_deps ) {
      cout << "BuildDep: " << builddep_package << endl;
    }
  }

  return 0;
}
