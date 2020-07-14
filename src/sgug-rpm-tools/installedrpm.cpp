#include "installedrpm.hpp"
#include "helpers.hpp"
#include "dependencyset.hpp"

#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

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
using std::unordered_set;
using std::unordered_map;

namespace sgug_rpm {

  installedrpm::installedrpm( string name,
			      string rpmfile,
			      vector<string> requires,
			      vector<string> provides )
    : _name(name),
      _rpmfile(rpmfile),
      _requires(requires),
      _provides(provides) {}

  bool read_installedrpm( const bool verbose, const string & packagename,
			  installedrpm & dest )
  {
    sgug_rpm::rpmts_h rpmts_helper;

    sgug_rpm::rpmtsiter_h iter_h( rpmts_helper, RPMTAG_NAME,
				  packagename.c_str(), 0 );

    Header installed_header;

    bool found_installed_package = false;
    while( (installed_header = iter_h.next()) != NULL ) {
      found_installed_package = true;
      installed_header = headerLink(installed_header);

      const char * errstr;
      char * dep_rpmfile_c = headerFormat(installed_header, "%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}.rpm", &errstr);
      string packagerpmfile(dep_rpmfile_c);

      if( verbose ) {
	cout << "# Checking deps of " << packagename <<
	  " rpm file is " << packagerpmfile << endl;
      }

      sgug_rpm::rpmds_h rpmds_prov(installed_header, RPMTAG_PROVIDENAME, 0);

      unordered_set<string> prov_set;
      if( rpmds_prov.dependency_set ) {
	while( rpmds_prov.next() >= 0 ) {
	  const char * DNEVR;
	  if((DNEVR = rpmdsDNEVR(rpmds_prov.dependency_set)) != NULL) {
	    string prov(DNEVR + 2);
	    if( strncmp(prov.c_str(), "rpmlib(", 7) == 0 ) {
	      continue;
	    }
	    if( prov_set.find(prov) != prov_set.end() ) {
	      //		cerr << "Warning: Dupe provide dep on package for " <<
	      //		  specfile.get_name() << ":" << pkg << ":" <<
	      //		     prov << endl;
	    }
	    else {
	      prov_set.insert(prov);
	    }
	  }
	}
      }
      vector<string> provides;
      for( const string & prov : prov_set ) {
	//	cout << "#  Provide: " << prov << endl;
	provides.emplace_back(prov);
      }

      sgug_rpm::rpmds_h rpmds_req(installed_header, RPMTAG_REQUIRENAME, 0);

      unordered_set<string> reqs_set;
      if( rpmds_req.dependency_set ) {
	while( rpmds_req.next() >= 0 ) {
	  const char * DNEVR;
	  if((DNEVR = rpmdsDNEVR(rpmds_req.dependency_set)) != NULL) {
	    const char * namestart = DNEVR + 2;
	    string req(DNEVR + 2);
	    // Remove any versioning
	    char * firstspace;
	    if( (firstspace=strstr(namestart, " ")) != NULL ) {
	      //	      cout << "Changing require from " << req << endl;
	      req = req.substr(0,firstspace-namestart);
	      //	      cout << "It is now " << req << endl;
	      if( str_starts_with(req,"(") && !str_ends_with(req, ")") ) {
		req = req.substr(1);
		//		exit(EXIT_FAILURE);
	      }
	    }
	    if( strncmp(req.c_str(), "rpmlib(", 7) == 0 ) {
	      continue;
	    }
	    if( reqs_set.find(req) != reqs_set.end() ) {
	      // Duplicate require dep on package - ignore it
	    }
	    else {
	      string short_req(req);
	      reqs_set.insert(short_req);
	    }
	  }
	}
      }
      vector<string> requires;
      for( const string & req : reqs_set ) {
	//	cout << "#  Require: " << req << endl;
	requires.emplace_back(req);	  
      }
      dest = installedrpm( packagename,
			   packagerpmfile,
			   requires,
			   provides );
    }
    return found_installed_package;
  }

  void read_installedrpms( const bool verbose,
			   const vector<string> & names,
			   vector<installedrpm> & out_instrpms,
			   vector<string> & error_instrpms )
  {
    for( const string & name : names ) {
      installedrpm one_instrpm;
      if( read_installedrpm( verbose, name, one_instrpm ) ) {
	out_instrpms.emplace_back( one_instrpm );
      }
      else {
	error_instrpms.push_back(name);
      }
    }
  }

}
