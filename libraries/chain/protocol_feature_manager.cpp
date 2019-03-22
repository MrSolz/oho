/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */

#include <eosio/chain/protocol_feature_manager.hpp>
#include <eosio/chain/protocol_state_object.hpp>
#include <eosio/chain/exceptions.hpp>

#include <fc/scoped_exit.hpp>

#include <algorithm>
#include <boost/assign/list_of.hpp>

namespace eosio { namespace chain {

   const std::unordered_map<builtin_protocol_feature_t, builtin_protocol_feature_spec, enum_hash<builtin_protocol_feature_t>>
   builtin_protocol_feature_codenames =
      boost::assign::map_list_of<builtin_protocol_feature_t, builtin_protocol_feature_spec>
         (  builtin_protocol_feature_t::preactivate_feature, builtin_protocol_feature_spec{
            "PREACTIVATE_FEATURE",
            fc::variant("64fe7df32e9b86be2b296b3f81dfd527f84e82b98e363bc97e40bc7a83733310").as<digest_type>(),
            // SHA256 hash of the raw message below within the comment delimiters (do not modify message below).
/*
Builtin protocol feature: PREACTIVATE_FEATURE

Adds privileged intrinsic to enable a contract to pre-activate a protocol feature specified by its digest.
Pre-activated protocol features must be activated in the next block.
*/
            {},
            {time_point{}, false, true} // enabled without preactivation and ready to go at any time
         } )
         (  builtin_protocol_feature_t::only_link_to_existing_permission, builtin_protocol_feature_spec{
            "ONLY_LINK_TO_EXISTING_PERMISSION",
            fc::variant("f3c3d91c4603cde2397268bfed4e662465293aab10cd9416db0d442b8cec2949").as<digest_type>(),
            // SHA256 hash of the raw message below within the comment delimiters (do not modify message below).
/*
Builtin protocol feature: ONLY_LINK_TO_EXISTING_PERMISSION

Disallows linking an action to a non-existing permission.
*/
            {}
         } )
   ;


   const char* builtin_protocol_feature_codename( builtin_protocol_feature_t codename ) {
      auto itr = builtin_protocol_feature_codenames.find( codename );
      EOS_ASSERT( itr != builtin_protocol_feature_codenames.end(), protocol_feature_validation_exception,
                  "Unsupported builtin_protocol_feature_t passed to builtin_protocol_feature_codename: ${codename}",
                  ("codename", static_cast<uint32_t>(codename)) );

      return itr->second.codename;
   }

   protocol_feature_base::protocol_feature_base( protocol_feature_t feature_type,
                                                 const digest_type& description_digest,
                                                 flat_set<digest_type>&& dependencies,
                                                 const protocol_feature_subjective_restrictions& restrictions )
   :description_digest( description_digest )
   ,dependencies( std::move(dependencies) )
   ,subjective_restrictions( restrictions )
   ,_type( feature_type )
   {
      switch( feature_type ) {
         case protocol_feature_t::builtin:
            protocol_feature_type = builtin_protocol_feature::feature_type_string;
         break;
         default:
         {
            EOS_THROW( protocol_feature_validation_exception,
                       "Unsupported protocol_feature_t passed to constructor: ${type}",
                       ("type", static_cast<uint32_t>(feature_type)) );
         }
         break;
      }
   }

   void protocol_feature_base::reflector_init() {
      static_assert( fc::raw::has_feature_reflector_init_on_unpacked_reflected_types,
                     "protocol_feature_activation expects FC to support reflector_init" );

      if( protocol_feature_type == builtin_protocol_feature::feature_type_string ) {
         _type = protocol_feature_t::builtin;
      } else {
         EOS_THROW( protocol_feature_validation_exception,
                    "Unsupported protocol feature type: ${type}", ("type", protocol_feature_type) );
      }
   }

   const char* builtin_protocol_feature::feature_type_string = "builtin";

   builtin_protocol_feature::builtin_protocol_feature( builtin_protocol_feature_t codename,
                                                       const digest_type& description_digest,
                                                       flat_set<digest_type>&& dependencies,
                                                       const protocol_feature_subjective_restrictions& restrictions )
   :protocol_feature_base( protocol_feature_t::builtin, description_digest, std::move(dependencies), restrictions )
   ,_codename(codename)
   {
      auto itr = builtin_protocol_feature_codenames.find( codename );
      EOS_ASSERT( itr != builtin_protocol_feature_codenames.end(), protocol_feature_validation_exception,
                  "Unsupported builtin_protocol_feature_t passed to constructor: ${codename}",
                  ("codename", static_cast<uint32_t>(codename)) );

      builtin_feature_codename = itr->second.codename;
   }

   void builtin_protocol_feature::reflector_init() {
      protocol_feature_base::reflector_init();

      for( const auto& p : builtin_protocol_feature_codenames ) {
         if( builtin_feature_codename.compare( p.second.codename ) == 0 ) {
            _codename = p.first;
            return;
         }
      }

      EOS_THROW( protocol_feature_validation_exception,
                 "Unsupported builtin protocol feature codename: ${codename}",
                 ("codename", builtin_feature_codename) );
   }


   digest_type builtin_protocol_feature::digest()const {
      digest_type::encoder enc;
      fc::raw::pack( enc, _type );
      fc::raw::pack( enc, description_digest  );
      fc::raw::pack( enc, dependencies );
      fc::raw::pack( enc, _codename );

      return enc.result();
   }

   fc::variant protocol_feature::to_variant( bool include_subjective_restrictions,
                                             fc::mutable_variant_object* additional_fields )const
   {
      EOS_ASSERT( builtin_feature, protocol_feature_exception, "not a builtin protocol feature" );

      fc::mutable_variant_object mvo;

      mvo( "feature_digest", feature_digest );

      if( additional_fields ) {
         for( const auto& e : *additional_fields ) {
            if( e.key().compare( "feature_digest" ) != 0 )
               mvo( e.key(), e.value() );
         }
      }

      if( include_subjective_restrictions ) {
         fc::mutable_variant_object subjective_restrictions;

         subjective_restrictions( "enabled", enabled );
         subjective_restrictions( "preactivation_required", preactivation_required );
         subjective_restrictions( "earliest_allowed_activation_time", earliest_allowed_activation_time );

         mvo( "subjective_restrictions", std::move( subjective_restrictions ) );
      }

      mvo( "description_digest", description_digest );
      mvo( "dependencies", dependencies );
      mvo( "protocol_feature_type", builtin_protocol_feature::feature_type_string );

      fc::variants specification;
      auto add_to_specification = [&specification]( const char* key_name, auto&& value ) {
         fc::mutable_variant_object obj;
         obj( "name", key_name );
         obj( "value", std::forward<decltype(value)>( value ) );
         specification.emplace_back( std::move(obj) );
      };


      add_to_specification( "builtin_feature_codename", builtin_protocol_feature_codename( *builtin_feature ) );

      mvo( "specification", std::move( specification ) );

      return fc::variant( std::move(mvo) );
   }

   protocol_feature_set::protocol_feature_set()
   {
      _recognized_builtin_protocol_features.reserve( builtin_protocol_feature_codenames.size() );
   }


   protocol_feature_set::recognized_t
   protocol_feature_set::is_recognized( const digest_type& feature_digest, time_point now )const {
      auto itr = _recognized_protocol_features.find( feature_digest );

      if( itr == _recognized_protocol_features.end() )
         return recognized_t::unrecognized;

      if( !itr->enabled )
         return recognized_t::disabled;

      if( itr->earliest_allowed_activation_time > now )
         return recognized_t::too_early;

      return recognized_t::ready;
   }

   optional<digest_type> protocol_feature_set::get_builtin_digest( builtin_protocol_feature_t feature_codename )const {
      uint32_t indx = static_cast<uint32_t>( feature_codename );

      if( indx >= _recognized_builtin_protocol_features.size() )
         return {};

      if( _recognized_builtin_protocol_features[indx] == _recognized_protocol_features.end() )
         return {};

      return _recognized_builtin_protocol_features[indx]->feature_digest;
   }

   const protocol_feature& protocol_feature_set::get_protocol_feature( const digest_type& feature_digest )const {
      auto itr = _recognized_protocol_features.find( feature_digest );

      EOS_ASSERT( itr != _recognized_protocol_features.end(), protocol_feature_exception,
                  "unrecognized protocol feature with digest: ${digest}",
                  ("digest", feature_digest)
      );

      return *itr;
   }

   bool protocol_feature_set::validate_dependencies(
                                    const digest_type& feature_digest,
                                    const std::function<bool(const digest_type&)>& validator
   )const {
      auto itr = _recognized_protocol_features.find( feature_digest );

      if( itr == _recognized_protocol_features.end() ) return false;

      for( const auto& d : itr->dependencies ) {
         if( !validator(d) ) return false;
      }

      return true;
   }

   builtin_protocol_feature
   protocol_feature_set::make_default_builtin_protocol_feature(
      builtin_protocol_feature_t codename,
      const std::function<digest_type(builtin_protocol_feature_t dependency)>& handle_dependency
   ) {
      auto itr = builtin_protocol_feature_codenames.find( codename );

      EOS_ASSERT( itr != builtin_protocol_feature_codenames.end(), protocol_feature_validation_exception,
                  "Unsupported builtin_protocol_feature_t: ${codename}",
                  ("codename", static_cast<uint32_t>(codename)) );

      flat_set<digest_type> dependencies;
      dependencies.reserve( itr->second.builtin_dependencies.size() );

      for( const auto& d : itr->second.builtin_dependencies ) {
         dependencies.insert( handle_dependency( d ) );
      }

      return {itr->first, itr->second.description_digest, std::move(dependencies), itr->second.subjective_restrictions};
   }

   const protocol_feature& protocol_feature_set::add_feature( const builtin_protocol_feature& f ) {
      auto builtin_itr = builtin_protocol_feature_codenames.find( f._codename );
      EOS_ASSERT( builtin_itr != builtin_protocol_feature_codenames.end(), protocol_feature_validation_exception,
                  "Builtin protocol feature has unsupported builtin_protocol_feature_t: ${codename}",
                  ("codename", static_cast<uint32_t>( f._codename )) );

      uint32_t indx = static_cast<uint32_t>( f._codename );

      if( indx < _recognized_builtin_protocol_features.size() ) {
         EOS_ASSERT( _recognized_builtin_protocol_features[indx] == _recognized_protocol_features.end(),
                     protocol_feature_exception,
                     "builtin protocol feature with codename '${codename}' already added",
                     ("codename", f.builtin_feature_codename) );
      }

      auto feature_digest = f.digest();

      const auto& expected_builtin_dependencies = builtin_itr->second.builtin_dependencies;
      flat_set<builtin_protocol_feature_t> satisfied_builtin_dependencies;
      satisfied_builtin_dependencies.reserve( expected_builtin_dependencies.size() );

      for( const auto& d : f.dependencies ) {
         auto itr = _recognized_protocol_features.find( d );
         EOS_ASSERT( itr != _recognized_protocol_features.end(), protocol_feature_exception,
            "builtin protocol feature with codename '${codename}' and digest of ${digest} has a dependency on a protocol feature with digest ${dependency_digest} that is not recognized",
            ("codename", f.builtin_feature_codename)
            ("digest",  feature_digest)
            ("dependency_digest", d )
         );

         if( itr->builtin_feature
             && expected_builtin_dependencies.find( *itr->builtin_feature )
                  != expected_builtin_dependencies.end() )
         {
            satisfied_builtin_dependencies.insert( *itr->builtin_feature );
         }
      }

      if( expected_builtin_dependencies.size() > satisfied_builtin_dependencies.size() ) {
         flat_set<builtin_protocol_feature_t> missing_builtins;
         missing_builtins.reserve( expected_builtin_dependencies.size() - satisfied_builtin_dependencies.size() );
         std::set_difference( expected_builtin_dependencies.begin(), expected_builtin_dependencies.end(),
                              satisfied_builtin_dependencies.begin(), satisfied_builtin_dependencies.end(),
                              end_inserter( missing_builtins )
         );

         vector<string> missing_builtins_with_names;
         missing_builtins_with_names.reserve( missing_builtins.size() );
         for( const auto& builtin_codename : missing_builtins ) {
            auto itr = builtin_protocol_feature_codenames.find( builtin_codename );
            EOS_ASSERT( itr != builtin_protocol_feature_codenames.end(),
                        protocol_feature_exception,
                        "Unexpected error"
            );
            missing_builtins_with_names.emplace_back( itr->second.codename );
         }

         EOS_THROW(  protocol_feature_validation_exception,
                     "Not all the builtin dependencies of the builtin protocol feature with codename '${codename}' and digest of ${digest} were satisfied.",
                     ("missing_dependencies", missing_builtins_with_names)
         );
      }

      auto res = _recognized_protocol_features.insert( protocol_feature{
         feature_digest,
         f.description_digest,
         f.dependencies,
         f.subjective_restrictions.earliest_allowed_activation_time,
         f.subjective_restrictions.preactivation_required,
         f.subjective_restrictions.enabled,
         f._codename
      } );

      EOS_ASSERT( res.second, protocol_feature_exception,
                  "builtin protocol feature with codename '${codename}' has a digest of ${digest} but another protocol feature with the same digest has already been added",
                  ("codename", f.builtin_feature_codename)("digest", feature_digest) );

      if( indx >= _recognized_builtin_protocol_features.size() ) {
         for( auto i =_recognized_builtin_protocol_features.size(); i <= indx; ++i ) {
            _recognized_builtin_protocol_features.push_back( _recognized_protocol_features.end() );
         }
      }

      _recognized_builtin_protocol_features[indx] = res.first;
      return *res.first;
   }



   protocol_feature_manager::protocol_feature_manager( protocol_feature_set&& pfs )
   :_protocol_feature_set( std::move(pfs) )
   {
      _builtin_protocol_features.resize( _protocol_feature_set._recognized_builtin_protocol_features.size() );
   }

   void protocol_feature_manager::init( chainbase::database& db ) {
      EOS_ASSERT( !is_initialized(), protocol_feature_exception, "cannot initialize protocol_feature_manager twice" );


      auto reset_initialized = fc::make_scoped_exit( [this]() { _initialized = false; } );
      _initialized = true;

      for( const auto& f : db.get<protocol_state_object>().activated_protocol_features ) {
         activate_feature( f.feature_digest, f.activation_block_num );
      }

      reset_initialized.cancel();
   }

   const protocol_feature* protocol_feature_manager::const_iterator::get_pointer()const {
      //EOS_ASSERT( _pfm, protocol_feature_iterator_exception, "cannot dereference singular iterator" );
      //EOS_ASSERT( _index != end_index, protocol_feature_iterator_exception, "cannot dereference end iterator" );
      return &*(_pfm->_activated_protocol_features[_index].iterator_to_protocol_feature);
   }

   uint32_t protocol_feature_manager::const_iterator::activation_ordinal()const {
      EOS_ASSERT( _pfm,
                   protocol_feature_iterator_exception,
                  "called activation_ordinal() on singular iterator"
      );
      EOS_ASSERT( _index != end_index,
                   protocol_feature_iterator_exception,
                  "called activation_ordinal() on end iterator"
      );

      return _index;
   }

   uint32_t protocol_feature_manager::const_iterator::activation_block_num()const {
      EOS_ASSERT( _pfm,
                   protocol_feature_iterator_exception,
                  "called activation_block_num() on singular iterator"
      );
      EOS_ASSERT( _index != end_index,
                   protocol_feature_iterator_exception,
                  "called activation_block_num() on end iterator"
      );

      return _pfm->_activated_protocol_features[_index].activation_block_num;
   }

   protocol_feature_manager::const_iterator& protocol_feature_manager::const_iterator::operator++() {
      EOS_ASSERT( _pfm, protocol_feature_iterator_exception, "cannot increment singular iterator" );
      EOS_ASSERT( _index != end_index, protocol_feature_iterator_exception, "cannot increment end iterator" );

      ++_index;
      if( _index >= _pfm->_activated_protocol_features.size() ) {
         _index = end_index;
      }

      return *this;
   }

   protocol_feature_manager::const_iterator& protocol_feature_manager::const_iterator::operator--() {
      EOS_ASSERT( _pfm, protocol_feature_iterator_exception, "cannot decrement singular iterator" );
      if( _index == end_index ) {
         EOS_ASSERT( _pfm->_activated_protocol_features.size() > 0,
                     protocol_feature_iterator_exception,
                     "cannot decrement end iterator when no protocol features have been activated"
         );
         _index = _pfm->_activated_protocol_features.size() - 1;
      } else {
         EOS_ASSERT( _index > 0,
                     protocol_feature_iterator_exception,
                     "cannot decrement iterator at the beginning of protocol feature activation list" )
         ;
         --_index;
      }
      return *this;
   }

   protocol_feature_manager::const_iterator protocol_feature_manager::cbegin()const {
      if( _activated_protocol_features.size() == 0 ) {
         return cend();
      } else {
         return const_iterator( this, 0 );
      }
   }

   protocol_feature_manager::const_iterator
   protocol_feature_manager::at_activation_ordinal( uint32_t activation_ordinal )const {
      if( activation_ordinal >= _activated_protocol_features.size() ) {
         return cend();
      }

      return const_iterator{this, static_cast<std::size_t>(activation_ordinal)};
   }

   protocol_feature_manager::const_iterator
   protocol_feature_manager::lower_bound( uint32_t block_num )const {
      const auto begin = _activated_protocol_features.cbegin();
      const auto end   = _activated_protocol_features.cend();
      auto itr = std::lower_bound( begin, end, block_num, []( const protocol_feature_entry& lhs, uint32_t rhs ) {
         return lhs.activation_block_num < rhs;
      } );

      if( itr == end ) {
         return cend();
      }

      return const_iterator{this, static_cast<std::size_t>(itr - begin)};
   }

   protocol_feature_manager::const_iterator
   protocol_feature_manager::upper_bound( uint32_t block_num )const {
      const auto begin = _activated_protocol_features.cbegin();
      const auto end   = _activated_protocol_features.cend();
      auto itr = std::upper_bound( begin, end, block_num, []( uint32_t lhs, const protocol_feature_entry& rhs ) {
         return lhs < rhs.activation_block_num;
      } );

      if( itr == end ) {
         return cend();
      }

      return const_iterator{this, static_cast<std::size_t>(itr - begin)};
   }

   bool protocol_feature_manager::is_builtin_activated( builtin_protocol_feature_t feature_codename,
                                                        uint32_t current_block_num )const
   {
      uint32_t indx = static_cast<uint32_t>( feature_codename );

      if( indx >= _builtin_protocol_features.size() ) return false;

      return (_builtin_protocol_features[indx].activation_block_num <= current_block_num);
   }

   void protocol_feature_manager::activate_feature( const digest_type& feature_digest,
                                                    uint32_t current_block_num )
   {
      EOS_ASSERT( is_initialized(), protocol_feature_exception, "protocol_feature_manager is not yet initialized" );

      auto itr = _protocol_feature_set.find( feature_digest );

      EOS_ASSERT( itr != _protocol_feature_set.end(), protocol_feature_exception,
                  "unrecognized protocol feature digest: ${digest}", ("digest", feature_digest) );

      if( _activated_protocol_features.size() > 0 ) {
         const auto& last = _activated_protocol_features.back();
         EOS_ASSERT( last.activation_block_num <= current_block_num,
                     protocol_feature_exception,
                     "last protocol feature activation block num is ${last_activation_block_num} yet "
                     "attempting to activate protocol feature with a current block num of ${current_block_num}"
                     "protocol features is ${last_activation_block_num}",
                     ("current_block_num", current_block_num)
                     ("last_activation_block_num", last.activation_block_num)
         );
      }

      EOS_ASSERT( itr->builtin_feature,
                  protocol_feature_exception,
                  "invariant failure: encountered non-builtin protocol feature which is not yet supported"
      );

      uint32_t indx = static_cast<uint32_t>( *itr->builtin_feature );

      EOS_ASSERT( indx < _builtin_protocol_features.size(), protocol_feature_exception,
                  "invariant failure while trying to activate feature with digest '${digest}': "
                  "unsupported builtin_protocol_feature_t ${codename}",
                  ("digest", feature_digest)
                  ("codename", indx)
      );

      EOS_ASSERT( _builtin_protocol_features[indx].activation_block_num == builtin_protocol_feature_entry::not_active,
                  protocol_feature_exception,
                  "cannot activate already activated builtin feature with digest: ${digest}",
                  ("digest", feature_digest)
      );

      _activated_protocol_features.push_back( protocol_feature_entry{itr, current_block_num} );
      _builtin_protocol_features[indx].previous = _head_of_builtin_activation_list;
      _builtin_protocol_features[indx].activation_block_num = current_block_num;
      _head_of_builtin_activation_list = indx;
   }

   void protocol_feature_manager::popped_blocks_to( uint32_t block_num ) {
      EOS_ASSERT( is_initialized(), protocol_feature_exception, "protocol_feature_manager is not yet initialized" );

      while( _head_of_builtin_activation_list != builtin_protocol_feature_entry::no_previous ) {
         auto& e = _builtin_protocol_features[_head_of_builtin_activation_list];
         if( e.activation_block_num <= block_num ) break;

         _head_of_builtin_activation_list = e.previous;
         e.previous = builtin_protocol_feature_entry::no_previous;
         e.activation_block_num = builtin_protocol_feature_entry::not_active;
      }

      while( _activated_protocol_features.size() > 0
              && block_num < _activated_protocol_features.back().activation_block_num )
      {
         _activated_protocol_features.pop_back();
      }
   }

} }  // eosio::chain
