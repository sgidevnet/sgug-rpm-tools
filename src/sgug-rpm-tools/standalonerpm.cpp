#include "standalonerpm.hpp"
#include "helpers.hpp"
#include "dependencyset.hpp"

#include <cstdio>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include <fcntl.h>

// rpm bits
#include <rpm/rpmlib.h>
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
using std::unordered_set;
using std::unordered_map;

namespace sgug_rpm {

  standalonerpm::standalonerpm( string name,
				string rpmfile,
				vector<string> provides,
				vector<string> requires )
    : _name(name),
      _rpmfile(rpmfile),
      _provides(provides),
      _requires(requires) {}

  bool read_standalonerpm( const bool verbose, const string & rpmpath,
			   standalonerpm & dest )
  {
    return read_standalonerpm( verbose, rpmpath, dest, false );
  }

  bool read_standalonerpm( const bool verbose, const string & rpmpath,
			   standalonerpm & dest,
			   const bool read_deps )
  {
    bool returnCode = false;
    Header h, sig;
    FD_t fd;
    rpmRC rc;
    const char * rpmpath_c_str = rpmpath.c_str();
    fd = Fopen(rpmpath_c_str, "r.ufdio");
    if( (!fd) || Ferror(fd) ) {
      cerr << "Failed Fopen" << endl;
      return false;
    }

    rpmts_h rpmts_helper;

    rc = rpmReadPackageFile( rpmts_helper.ts, fd, rpmpath_c_str, &h);
    if (rc != RPMRC_OK) {
      cerr << "Failed rpmReadPackageFile" << endl;
      Fclose(fd);
      return false;
    }

    const char * name = headerGetString(h, RPMTAG_NAME);

    vector<string> provides;
    vector<string> requires;

    if( read_deps ) {
      sgug_rpm::rpmds_read_deps( h,
				 provides,
				 requires );
    }

    dest = {string(name), rpmpath, provides, requires };

    returnCode = true;
    
    h = headerFree(h);

    Fclose(fd);

    return returnCode;
  }

  void read_standalonerpms( const bool verbose,
			   const vector<string> & rpmpaths,
			   vector<standalonerpm> & out_rpms,
			   vector<string> & error_rpms )
  {
    for( const string & rpmpath : rpmpaths ) {
      standalonerpm one_rpm;
      if( read_standalonerpm( verbose, rpmpath, one_rpm ) ) {
	out_rpms.emplace_back( one_rpm );
      }
      else {
	error_rpms.push_back(rpmpath);
      }
    }
  }

}
