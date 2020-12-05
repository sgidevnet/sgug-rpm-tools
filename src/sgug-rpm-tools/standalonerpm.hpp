#ifndef STANDALONERPM_HPP
#define STANDALONERPM_HPP

#include "helpers.hpp"

#include <string>
#include <vector>
#include <unordered_map>

namespace sgug_rpm {
  class standalonerpm {
  private:
    std::string _name;
    std::string _rpmfile;

    std::vector<std::string> _provides;
    std::vector<std::string> _requires;

  public:
    standalonerpm() {};
    standalonerpm( std::string name,
		   std::string rpmfile,
		   std::vector<std::string> provides,
		   std::vector<std::string> requires );
    const std::string & get_name() const { return _name; };
    const std::string & get_rpmfile() const { return _rpmfile; };
    const std::vector<std::string> & get_provides() const { return _provides; };
    const std::vector<std::string> & get_requires() const { return _requires; };
  };

  bool read_standalonerpm( const bool verbose,
			   const std::string & rpmpath,
			   standalonerpm & dest );

  bool read_standalonerpm( const bool verbose,
			   const std::string & rpmpath,
			   standalonerpm & dest,
			   const bool read_deps );

  void read_standalonerpms( const bool verbose,
			    const std::vector<std::string> & rpmpaths,
			    std::vector<standalonerpm> & out_rpms,
			    std::vector<std::string> & error_rpms );
}

#endif
