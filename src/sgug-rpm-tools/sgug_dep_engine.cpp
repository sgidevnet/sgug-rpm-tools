#include "sgug_dep_engine.hpp"

#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <algorithm>
#include <functional>
#include <utility>

#include <iostream>
#include <sstream>

using std::function;
using std::optional;
using std::pair;
using std::reference_wrapper;
using std::stack;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::unordered_set;
using std::vector;

using std::endl;
using std::cout;

namespace sgug_rpm {

  static optional<uint32_t> recursive_flatten_deps(
					  vector<string> & missing_deps_out,
					  resolvedrpm & driving_pkg,
					  vector<resolvedrpm> & known_packages,
					  unordered_map<string,std::reference_wrapper<resolvedrpm> > & pid_to_package,
					  unordered_map<string,std::reference_wrapper<resolvedrpm> > & provides_to_package,
					  unordered_set<string> & done_packages,
					  resolvedrpm & current_pkg,
					  vector<string> & pkg_resolution_stack,
					  const function<bool (const string&)> & special_strategy,
					  bool parent_is_special,
					  progress_printer & pprinter )
  {
    const string & pkg_name = current_pkg.get_package().get_name();

    //    cout << "do irfp on " << pkg_name << endl;

    if( done_packages.find(pkg_name) != done_packages.end() ) {
      //      cout << "RPM " << pkg_name << " already processed." << endl;
      auto pfinder = pid_to_package.find(pkg_name);
      return pfinder->second.get().get_sequence_no();
    }

    bool is_special = special_strategy(pkg_name) || parent_is_special;

    //    for( const string & sname : pkg_resolution_stack ) {
    //      cout << "PRS: " << sname << endl;
    //    }

    if( std::find(pkg_resolution_stack.begin(), pkg_resolution_stack.end(), pkg_name)
	!= pkg_resolution_stack.end() ) {
      //      cout << "Dependency cycle detected with driving pkg: " <<
      //	pkg_name << endl;
      //      for( const string & spkg : pkg_resolution_stack ) {
      //	cout << "Stack: " << spkg << endl;
      //      }
      auto pfinder = pid_to_package.find(pkg_name);
      if( pfinder != pid_to_package.end() ) {
	return pfinder->second.get().get_sequence_no();
      }
      else {
	return {};
      }
    }
    pkg_resolution_stack.push_back(pkg_name);

    const vector<string> & pkg_requires = current_pkg.get_package().get_requires();

    uint32_t sequence_no = 0;

    for( const string & pkg_require : pkg_requires ) {
      //      cout << "Looking for provider of " << pkg_require << endl;
      optional<reference_wrapper<resolvedrpm> > provider_ref_opt;

      auto piter = provides_to_package.find(pkg_require);

      if( piter != provides_to_package.end() ) {
	resolvedrpm & child_pkg = piter->second.get();
	provider_ref_opt = {child_pkg};
      }

      if( !provider_ref_opt ) {
	/* Didn't resolve from simple prov/req */
	optional<pair<string,string>> provider_pkg_name_opt =
	  find_package_providing_file( pkg_require );

	if( provider_pkg_name_opt ) {
	  string & provider_pkg_name = (*provider_pkg_name_opt).first;
	  auto p2pfind = pid_to_package.find( provider_pkg_name );
	  if( p2pfind != pid_to_package.end() ) {
	    resolvedrpm & child_pkg = p2pfind->second.get();
	    provider_ref_opt = {child_pkg};
	  }
	}
      }

      if( !provider_ref_opt ) {
	/* Didn't resolve from simple prov/req */
	optional<pair<string,string>> provider_pkg_tag_opt =
	  find_package_providing_tag( pkg_require );

	if( provider_pkg_tag_opt ) {
	  string & provider_pkg_name = (*provider_pkg_tag_opt).first;
	  auto p2pfind = pid_to_package.find( provider_pkg_name );
	  if( p2pfind != pid_to_package.end() ) {
	    resolvedrpm & child_pkg = p2pfind->second.get();
	    provider_ref_opt = {child_pkg};
	  }
	}
      }

      if( !provider_ref_opt ) {
	stringstream missing_deps_buf;
	missing_deps_buf << "Package " << pkg_name << " has missing requires: " << pkg_require;
	missing_deps_out.push_back(missing_deps_buf.str());
	continue;
      }

      resolvedrpm & child_pkg = *provider_ref_opt;

      //      cout << "Found provider for " << pkg_require << endl;

      const string & child_pkg_name = child_pkg.get_package().get_name();

      if( child_pkg_name == pkg_name ) {
	//	cout << "Found package depending on itself: " << child_pkg_name << endl;
	continue;
      }

      //      cout << "It is " << child_pkg.get_package().get_name() << endl;

      optional<uint32_t> child_sequence_no_opt =
	recursive_flatten_deps( missing_deps_out,
				driving_pkg,
				known_packages,
				pid_to_package,
				provides_to_package,
				done_packages,
				child_pkg,
				pkg_resolution_stack,
				special_strategy,
				is_special,
				pprinter );

      if( child_sequence_no_opt ) {
	uint32_t child_sequence_no = *child_sequence_no_opt;
	uint32_t tmp_sequence_no = child_sequence_no + 1;

	if( tmp_sequence_no > sequence_no ) {
	  //	  cout << "Upgrading " << pkg_name << " sequence from " << sequence_no <<
	  //	    " to " << tmp_sequence_no << endl;
	  sequence_no = tmp_sequence_no;
	}
      }

      pprinter.accept_progress();
    }

    current_pkg.set_sequence_no(sequence_no);
    current_pkg.set_special(is_special);
    if( done_packages.find(pkg_name) == done_packages.end() ) {
      done_packages.emplace(pkg_name);
    }

    pkg_resolution_stack.pop_back();

    return {sequence_no};
  }

  vector<resolvedrpm> flatten_sort_packages( vector<installedrpm> & rpms_to_resolve,
					     const function<bool (const string&)> & special_strategy,
					     vector<string> & missing_deps_out,
					     progress_printer & pprinter )
  {
    vector<resolvedrpm> retval;
    unordered_map<string,reference_wrapper<resolvedrpm> > pid_to_package;
    unordered_map<string,reference_wrapper<resolvedrpm> > provides_to_package;
    for( installedrpm & irpm : rpms_to_resolve ) {
      string rpm_name = irpm.get_name();
      retval.emplace_back( irpm, 0 );
    }

    // Now things are stable in memory, build the maps
    for( resolvedrpm & rrpm : retval ) {
      string rpm_name = rrpm.get_package().get_name();
      pid_to_package.emplace(rpm_name, rrpm);
      provides_to_package.emplace(rpm_name, rrpm);
      for( const string & provide : rrpm.get_package().get_provides() ) {
	provides_to_package.emplace(provide, rrpm);
	//	cout << "RPM " << rpm_name << " provides " << provide << endl;
      }
    }

    unordered_set<string> done_packages;

    // First pass, only "special" packages
    for( resolvedrpm & pkg : retval ) {
      const string & pkg_name = pkg.get_package().get_name();
      if( special_strategy(pkg_name) ) {
	vector<string> pkg_resolution_stack;
	//      cout << "do erfp on " << pkg.get_package().get_name() << endl;
	recursive_flatten_deps( missing_deps_out,
				pkg,
				retval,
				pid_to_package,
				provides_to_package,
				done_packages,
				pkg,
				pkg_resolution_stack,
				special_strategy,
				true, // by default, special parent
				pprinter );
	pprinter.accept_progress();
      }
    }


    // Second pass, non "special"
    for( resolvedrpm & pkg : retval ) {
      vector<string> pkg_resolution_stack;
      //      cout << "do erfp on " << pkg.get_package().get_name() << endl;
      recursive_flatten_deps( missing_deps_out,
			      pkg,
			      retval,
			      pid_to_package,
			      provides_to_package,
			      done_packages,
			      pkg,
			      pkg_resolution_stack,
			      special_strategy,
			      false, // by default, no special parent
			      pprinter );
      pprinter.accept_progress();
    }

    std::sort(retval.begin(),retval.end(),
	      []( const resolvedrpm & a, const resolvedrpm & b ) -> bool {
		// Sort them by sequenceNumber, then by name
		// (to ensure stable results)
		if( a.get_sequence_no() == b.get_sequence_no() ) {
		  return a.get_package().get_name() < b.get_package().get_name();
		}
		else {
		  return a.get_sequence_no() < b.get_sequence_no();
		}
	      });

    pprinter.reset();

    return retval;
  }

}
