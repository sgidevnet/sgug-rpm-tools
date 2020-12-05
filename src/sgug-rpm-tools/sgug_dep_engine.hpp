#ifndef SGUG_DEP_ENGINE_HPP
#define SGUG_DEP_ENGINE_HPP

#include "helpers.hpp"
#include "installedrpm.hpp"

#include <vector>
#include <functional>

namespace sgug_rpm {

  class resolvedrpm {
    installedrpm _package;
    uint32_t _sequence_no;
    bool _special;

  public:
    resolvedrpm( const installedrpm & package,
		 uint32_t sequence_no )
      : _package( package),
	_sequence_no( sequence_no ),
	_special(false) {}

    const installedrpm & get_package() const { return _package; };

    const uint32_t get_sequence_no() const { return _sequence_no; };
    void set_sequence_no(uint32_t sequence_no) { _sequence_no = sequence_no; };

    const bool get_special() { return _special; };
    void set_special(bool new_value) { _special = new_value; };
  
  };

  std::vector<resolvedrpm> flatten_sort_packages( std::vector<installedrpm> & rpms_to_resolve,
						  const std::function<bool (const std::string&)> & special_strategy,
						  std::vector<std::string> & missing_deps_out,
						  progress_printer & pprinter );
}

#endif
