#include <dependencyset.hpp>

#include <string>
#include <vector>
#include <unordered_set>

#include <algorithm>

#include <iostream>

using std::string;
using std::vector;
using std::unordered_set;

namespace sgug_rpm
{
  void rpmds_read_deps( Header package_header,
			std::vector<std::string> & provides,
			std::vector<std::string> & requires ) {
    // Provides
    rpmds_h rpmds_prov( package_header, RPMTAG_PROVIDENAME, 0);
    unordered_set<string> prov_set;
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
    for( const string & s : prov_set ) {
      provides.push_back(s);
      //      std::cout << "Found provide: " << s << std::endl;
    }
    // Requires
    rpmds_h rpmds_req( package_header, RPMTAG_REQUIRENAME, 0);
    unordered_set<string> req_set;
    while( rpmds_req.next() >= 0 ) {
      const char * DNEVR;
      if((DNEVR = rpmdsDNEVR(rpmds_req.dependency_set)) != NULL) {
	string req(DNEVR + 2);
	if( strncmp(req.c_str(), "rpmlib(", 7) == 0 ) {
	  continue;
	}
	if( req_set.find(req) != req_set.end() ) {
	  //		cerr << "Warning: Dupe require dep on package for " <<
	  //		  specfile.get_name() << ":" << pkg << ":" <<
	  //		     req << endl;
	}
	else {
	  req_set.insert(req);
	}
      }
    }
    for( const string & s : req_set ) {
      requires.push_back(s);
      //      std::cout << "Found require: " << s << std::endl;
    }
  }

}
