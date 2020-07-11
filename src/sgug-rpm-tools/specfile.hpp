#ifndef SPECFILES_HPP
#define SPECFILES_HPP

#include "helpers.hpp"

#include <string>
#include <vector>
#include <unordered_map>

#include <rpm/rpmspec.h>

namespace sgug_rpm {
  class specfile {
  private:
    std::string _filepath;
    std::string _name;

    std::vector<std::string> _packages;
    std::unordered_map<std::string,std::vector<std::string>> _package_deps;

  public:
    specfile() {};
    specfile( std::string filepath,
	      std::string name,
	      std::vector<std::string> packages,
	      std::unordered_map<std::string,std::vector<std::string>> package_deps );
    const std::string & get_filepath() const { return _filepath; };
    const std::string & get_name() const { return _name; };
    const std::vector<std::string> & get_packages() const { return _packages; };
    const std::unordered_map<std::string,std::vector<std::string>> & get_package_deps() const { return _package_deps; };
  };

  bool read_specfile( const std::string & path,
		      rpmSpecFlags flags,
		      specfile & dest,
		      sgug_rpm::progress_printer & pprinter );

  void read_specfiles( poptcontext_h & popt_context,
		       const std::vector<std::string> & paths,
		       rpmSpecFlags flags,
		       std::vector<specfile> & out_specfiles,
		       std::vector<std::string> & error_specfiles,
		       sgug_rpm::progress_printer & pprinter );

  void read_rpmbuild_specfiles( sgug_rpm::poptcontext_h & popt_context,
				rpmSpecFlags flags,
				std::vector<specfile> & out_specfiles,
				std::vector<std::string> & error_specfiles,
				sgug_rpm::progress_printer & pprinter );
}

#endif
