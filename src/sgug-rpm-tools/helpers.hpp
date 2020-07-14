#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <rpm/rpmcli.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>

#include <string>
#include <optional>
#include <utility>

namespace sgug_rpm {
  class poptcontext_h {
  public:
    poptContext context;
    bool verbose;
    
    poptcontext_h( int argc, char ** argv,
		 struct poptOption optionsTable[]) {
      context = rpmcliInit(argc, argv, optionsTable);
      verbose = rpmIsVerbose();
    }

    void reset_rpm_macros() {
      rpmFreeMacros(NULL);
      rpmReadConfigFiles(NULL, NULL);
    }

    ~poptcontext_h() {
      context = rpmcliFini(context);
    }
  };

  class rpmts_h {
  public:
    rpmts ts;

    rpmts_h() {
      ts = rpmtsCreate();
    }

    ~rpmts_h() {
      ts = rpmtsFree(ts);
    }
  };

  class rpmtsiter_h {
  public:
    rpmdbMatchIterator iter;

    rpmtsiter_h(rpmts_h & rpmts_a, rpmDbiTagVal rpmtag,
		const void * keyp, size_t keylen ) {
      iter = rpmtsInitIterator(rpmts_a.ts,rpmtag,keyp,keylen);
    }

    Header next() { return rpmdbNextIterator(iter); };

    ~rpmtsiter_h() {
      iter = rpmdbFreeIterator(iter);
    }
  };

  inline bool str_ends_with( const std::string & str, const std::string & suf ) {
    return str.size() >= suf.size() &&
      0 == str.compare(str.size()-suf.size(), suf.size(), suf);
  }

  inline bool str_starts_with( const std::string & str, const std::string & pre ) {
    return str.size() >= pre.size() &&
      0 == str.compare(0, pre.size(), pre);
  }

  std::optional<std::pair<std::string,std::string> >
  find_package_providing_file( const std::string & required );
  std::optional<std::pair<std::string,std::string> >
  find_package_providing_tag( const std::string & required );

  class progress_printer {
    uint32_t prev_value;
  public:
    progress_printer();
    void accept_progress();
    void reset();
  };
}

#endif
