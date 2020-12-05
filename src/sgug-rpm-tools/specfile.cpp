#include "specfile.hpp"

#include <iostream>
#include <filesystem>

// rpm bits
#include <rpm/rpmcli.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmts.h>
#include <rpm/rpmarchive.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmspec.h>
#include <rpm/rpmbuild.h>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::unordered_map;

using std::filesystem::path;

namespace fs = std::filesystem;

namespace sgug_rpm {

  class rpmspec_h {
  public:
    rpmSpec this_spec;

    rpmspec_h( const char * specFile, rpmSpecFlags flags,
	       const char * buildRoot ) {
      this_spec = rpmSpecParse(specFile, flags, buildRoot );
    }

    ~rpmspec_h() {
      this_spec = rpmSpecFree(this_spec);
    }
  };

  class rpmspecpkgiter_h {
  public:
    rpmSpecPkgIter spec_pkg_iter;

    rpmspecpkgiter_h( rpmspec_h & spec_h ) {
      spec_pkg_iter = rpmSpecPkgIterInit(spec_h.this_spec);
    }

    rpmSpecPkg next() { return rpmSpecPkgIterNext(spec_pkg_iter); }

    ~rpmspecpkgiter_h() {
      rpmSpecPkgIterFree(spec_pkg_iter);
    }
  };

  specfile::specfile( string filepath,
		      string name,
		      vector<string> packages,
		      unordered_map<string,vector<string>> build_deps )
    : _filepath(filepath),
      _name(name),
      _packages(packages),
      _build_deps(build_deps) {}

  bool read_specfile( const string & path,
		      rpmSpecFlags flags,
		      specfile & dest,
		      progress_printer & pprinter )
  {
    rpmspec_h spec_h( path.c_str(), flags, NULL );
    if( !spec_h.this_spec ) {
      pprinter.reset();
      cerr << "Failed parsing spec: " << path << endl;
      return false;
    }
    string spec_name;

    rpmspecpkgiter_h specpkgiter_h( spec_h );
    rpmSpecPkg spec_pkg;
    bool first=true;
    vector<string> packages;
    unordered_map<string, vector<string>> build_deps;
    while((spec_pkg = specpkgiter_h.next()) != NULL ) {
      
      Header spec_header = rpmSpecPkgHeader(spec_pkg);
      const char * name_tag = headerGetString(spec_header,RPMTAG_NAME);
      string pkg_name = string(name_tag);
      if(first) {
	spec_name = pkg_name;
	first = false;
      }
      packages.push_back(pkg_name);
      build_deps.emplace(pkg_name, vector<string>());
    }

    dest = specfile{ path, spec_name, packages, build_deps };

    return true;
  }

  void read_specfiles( poptcontext_h & popt_context,
		       const vector<string> & paths,
		       rpmSpecFlags flags,
		       vector<specfile> & out_specfiles,
		       vector<string> & error_specfiles,
		       progress_printer & pprinter )
  {
    for( const string & spec_filename : paths ) {
      specfile specfile;
      popt_context.reset_rpm_macros();
      if( read_specfile(spec_filename, flags, specfile, pprinter) ) {
	out_specfiles.push_back( specfile );
      }
      else {
	error_specfiles.push_back( spec_filename );
      }
      pprinter.accept_progress();
    }
    pprinter.reset();
  }

  void read_rpmbuild_specfiles( poptcontext_h & popt_context,
				rpmSpecFlags flags,
				vector<specfile> & out_specfiles,
				vector<string> & error_specfiles,
				progress_printer & pprinter )
  {
    path homedir_path = std::filesystem::path(getenv("HOME"));

    path rpmbuild_specs_path = homedir_path / "rpmbuild" / "SPECS";

    vector<string> found_specpaths;
    if( fs::exists(rpmbuild_specs_path) && fs::is_directory(rpmbuild_specs_path)) {
      for( const auto & entry : fs::directory_iterator(rpmbuild_specs_path) ) {
	path entry_path = entry.path();
	auto filename = entry_path.filename();
	if( str_ends_with(filename, ".spec") ) {
	  found_specpaths.emplace_back(fs::canonical(entry_path));
	}
      }
    }

    if( found_specpaths.size() > 0 ) {
      read_specfiles( popt_context,
		      found_specpaths,
		      flags,
		      out_specfiles,
		      error_specfiles,
		      pprinter );
    }
  }

}
