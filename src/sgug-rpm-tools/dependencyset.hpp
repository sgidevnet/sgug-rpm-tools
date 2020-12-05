#ifndef DEPENDENCYSET_HPP
#define DEPENDENCYSET_HPP

#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmts.h>

#include <vector>
#include <string>

namespace sgug_rpm {

  class rpmds_h {
  public:
    rpmds dependency_set;

    rpmds_h( Header installed_header, rpmTagVal tagN, int flags ) {
      dependency_set = rpmdsNew(installed_header, tagN, flags);
    }

    int next() { return rpmdsNext(dependency_set); };

    ~rpmds_h() {
      dependency_set = rpmdsFree(dependency_set);
    }
  };

  void rpmds_read_deps( Header package_header,
			std::vector<std::string> & provides,
			std::vector<std::string> & requires );

}

#endif
