// WCAP audit, WBP-2002: authorisation and behaviour tests for the guilds.oig contract.
//
// The contract under test is not built by this tree. tests/test_contracts/guilds/guilds_oig_718903f.{wasm,abi}
// are the artefacts of wax-office-of-inspector-general/guilds.oig commit 718903f ("Convert guilds.oig to
// asynchronous scoring-only contract") built with cdt 4.1.1wax01; the fixture pins their sha256 so the
// checked-in binary cannot drift from the audited build without this suite saying so.
//
// The legacy mock (guilds_oig.{wasm,abi}) stays untouched: eosio.weighted_producer_tests depends on its ABI.

#include <boost/test/unit_test.hpp>
#include <eosio/chain/account_object.hpp>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/exceptions.hpp>
#include <fc/crypto/sha256.hpp>
#include <algorithm>
#include <limits>
#include <map>
#include <set>

#include "eosio.system_tester.hpp"

using namespace eosio_system;

namespace wcap_guilds {

inline const name guilds_account = "guilds.oig"_n;
inline const name oig             = "oig"_n;
inline const name treasury        = "treasury.oig"_n;
inline const name alice           = "alice1111111"_n;   // exists in the base tester, never a producer
inline const name prod_a          = "defproducera"_n;
inline const name prod_b          = "defproducerb"_n;
inline const name prod_c          = "defproducerc"_n;
inline const name prod_d          = "defproducerd"_n;
inline const name prod_e          = "defproducere"_n;

// sha256 of guilds_oig_718903f.wasm. Move this constant only in a commit that names the guilds.oig source SHA.
inline const std::string guilds_718903f_sha256     = "cca52864dcbb13af50069ad5358c3a3d0254d351e6d79bac3b8c1d4ea3ded155";
// sha256 of guilds_oig_718903f.abi (the file bytes; read_abi's trailing NUL is excluded from the hash)
inline const std::string guilds_718903f_abi_sha256 = "2e0f556559d7941a388f15540be530ecd66b912e03cdf8b1cf3c75ae9fcaf300";

inline const std::string lacking_oig_authority = "Lacking OIG authority.";
inline const std::string empty_evaluation      = "Evaluation must include at least one guild score.";
inline const std::string score_ceiling         = "Guild score must not exceed 100,000,000.";
inline const std::string not_a_producer        = "Account not a registered producer!";
inline const std::string unknown_guild         = "Unknown Guild in vector.";
inline const std::string guild_not_retired     = "Guild not retired.";

constexpr uint32_t max_score = 100000000;

using score_list = std::vector<std::pair<name, uint32_t>>;

// Deploys one guilds build onto guilds.oig, creates oig / treasury.oig, registers defproducera..e.
struct guilds_fixture_base : eosio_system_tester {
   abi_serializer          guilds_abi_ser;
   std::vector<uint8_t>    deployed_wasm;
   std::vector<char>       deployed_abi;
   const std::vector<name> producers = { prod_a, prod_b, prod_c, prod_d, prod_e };

   guilds_fixture_base( std::vector<uint8_t> wasm, std::vector<char> abi )
      : deployed_wasm( std::move(wasm) ), deployed_abi( std::move(abi) ) {
      const asset net = core_sym::from_string("800.0000");
      const asset cpu = core_sym::from_string("800.0000");
      for( const auto& a : { guilds_account, oig, treasury } ) {
         create_account_with_resources( a, config::system_account_name, core_sym::from_string("100.0000"), false, net, cpu );
         transfer( config::system_account_name, a, core_sym::from_string("1000000.0000"), config::system_account_name );
         BOOST_REQUIRE_EQUAL( success(), stake( a, core_sym::from_string("300000.0000"), core_sym::from_string("300000.0000") ) );
      }
      produce_blocks( 2 );

      set_code( guilds_account, deployed_wasm );
      set_abi( guilds_account, deployed_abi.data() );
      {
         const auto& accnt = control->db().get<account_object,by_name>( guilds_account );
         abi_def abi_def_;
         BOOST_REQUIRE_EQUAL( abi_serializer::to_abi( accnt.abi, abi_def_ ), true );
         guilds_abi_ser.set_abi( abi_def_, abi_serializer::create_yield_function( abi_serializer_max_time ) );
      }

      setup_producer_accounts( producers );
      for( const auto& p : producers ) {
         BOOST_REQUIRE_EQUAL( success(), regproducer( p ) );
      }
      produce_blocks( 1 );
   }

   std::string wasm_sha256() const {
      return fc::sha256::hash( reinterpret_cast<const char*>( deployed_wasm.data() ), deployed_wasm.size() ).str();
   }

   std::string abi_sha256() const {
      const auto end = std::find( deployed_abi.begin(), deployed_abi.end(), '\0' );
      return fc::sha256::hash( deployed_abi.data(), static_cast<uint32_t>( end - deployed_abi.begin() ) ).str();
   }

   std::string on_chain_code_hash() const {
      return control->db().get<account_metadata_object,by_name>( guilds_account ).code_hash.str();
   }

   std::set<std::string> abi_action_names() const {
      std::set<std::string> out;
      const auto& accnt = control->db().get<account_object,by_name>( guilds_account );
      abi_def abi;
      BOOST_REQUIRE_EQUAL( abi_serializer::to_abi( accnt.abi, abi ), true );
      for( const auto& a : abi.actions ) out.insert( a.name.to_string() );
      return out;
   }

   // --- actions -------------------------------------------------------------------------------------

   action make_guild_action( const action_name& act_name, const variant_object& data ) {
      action act;
      act.account = guilds_account;
      act.name    = act_name;
      act.data    = guilds_abi_ser.variant_to_binary( guilds_abi_ser.get_action_type( act_name ), data,
                                                      abi_serializer::create_yield_function( abi_serializer_max_time ) );
      return act;
   }

   // signer@active, signed with signer's active key (the base_tester convention)
   action_result push_guild_action( const name& signer, const action_name& act_name, const variant_object& data ) {
      return base_tester::push_action( make_guild_action( act_name, data ), signer.to_uint64_t() );
   }

   // one action, an explicit authorization line, signed with an explicit key
   action_result push_guild_action_as( const permission_level& auth, const name& key_owner, const std::string& key_role,
                                       const action_name& act_name, const variant_object& data ) {
      action act = make_guild_action( act_name, data );
      act.authorization = { auth };
      signed_transaction trx;
      trx.actions.emplace_back( std::move(act) );
      set_transaction_headers( trx );
      trx.sign( get_private_key( key_owner, key_role ), control->get_chain_id() );
      try {
         push_transaction( trx );
      } catch( const fc::exception& ex ) {
         return error( ex.top_message() );
      }
      produce_block();
      BOOST_REQUIRE_EQUAL( true, chain_has_transaction( trx.id() ) );
      return success();
   }

   static fc::variants scores_variant( const score_list& scores ) {
      fc::variants out;
      for( const auto& [guild, score] : scores ) out.push_back( mvo()("guild", guild)("score", score) );
      return out;
   }

   action_result pusheval( const name& signer, uint8_t type, const score_list& scores ) {
      return push_guild_action( signer, "pusheval"_n, mvo()("type", type)("scores", scores_variant( scores )) );
   }
   action_result retireguild( const name& signer, const std::vector<name>& guilds ) {
      return push_guild_action( signer, "retireguild"_n, mvo()("producer", guilds) );
   }
   action_result rmguild( const name& signer, const std::vector<name>& guilds ) {
      return push_guild_action( signer, "rmguild"_n, mvo()("producer", guilds) );
   }

   // --- tables --------------------------------------------------------------------------------------

   std::vector<char> guild_raw( const name& producer ) {
      return get_row_by_account( guilds_account, guilds_account, "guilds"_n, producer );
   }
   bool has_guild( const name& producer ) { return !guild_raw( producer ).empty(); }

   fc::variant guild( const name& producer ) {
      auto data = guild_raw( producer );
      BOOST_REQUIRE_MESSAGE( !data.empty(), "no guilds row for " + producer.to_string() );
      return guilds_abi_ser.binary_to_variant( "guild", data, abi_serializer::create_yield_function( abi_serializer_max_time ) );
   }
   uint32_t score( const name& producer )     { return guild( producer )["score"].as<uint32_t>(); }
   uint32_t prv_score( const name& producer ) { return guild( producer )["prv_score"].as<uint32_t>(); }
   bool     retired( const name& producer )   { return guild( producer )["retired"].as<bool>(); }

   std::vector<fc::variant> table_rows( const name& table, const std::string& type ) {
      std::vector<fc::variant> out;
      const auto* tid = control->db().find<table_id_object, by_code_scope_table>(
         boost::make_tuple( guilds_account, guilds_account, table ) );
      if( !tid ) return out;
      const auto& idx = control->db().get_index<key_value_index, by_scope_primary>();
      for( auto itr = idx.lower_bound( boost::make_tuple( tid->id, 0 ) ); itr != idx.end() && itr->t_id == tid->id; ++itr ) {
         std::vector<char> data( itr->value.data(), itr->value.data() + itr->value.size() );
         out.push_back( guilds_abi_ser.binary_to_variant( type, data, abi_serializer::create_yield_function( abi_serializer_max_time ) ) );
      }
      return out;
   }
   std::vector<fc::variant> evaluations() { return table_rows( "evaluations"_n, "evaluation" ); }
   size_t evaluation_count() { return evaluations().size(); }
   size_t guild_count()      { return table_rows( "guilds"_n, "guild" ).size(); }

   int64_t ram_usage( const name& account ) {
      return control->get_resource_limits_manager().get_account_ram_usage( account );
   }

   // snapshot of every guilds row, for "nothing else moved" assertions
   std::map<name, std::vector<char>> guild_snapshot( const std::vector<name>& names ) {
      std::map<name, std::vector<char>> out;
      for( const auto& n : names ) out[n] = guild_raw( n );
      return out;
   }
   void require_unchanged( const std::map<name, std::vector<char>>& before ) {
      for( const auto& [n, bytes] : before ) {
         BOOST_REQUIRE_MESSAGE( guild_raw( n ) == bytes, "guilds row for " + n.to_string() + " changed" );
      }
   }
};

// the audited build
struct eosio_guilds_tester : guilds_fixture_base {
   eosio_guilds_tester() : guilds_fixture_base( contracts::util::guild_718903f_wasm(), contracts::util::guild_718903f_abi() ) {
      BOOST_REQUIRE_EQUAL( guilds_718903f_sha256, wasm_sha256() );
      BOOST_REQUIRE_EQUAL( guilds_718903f_abi_sha256, abi_sha256() );
   }
};

// the legacy mock the weighted-producer tests deploy (deployed-era shape: pusheval -> setscores -> clearscores,
// then apply() -> vote()). apply() asserts top21.size()==21 ("Less than 21 guilds passing minimum score.") but
// fills top21 with 20 names for i<20 and adds the 21st only in the i==21 branch, so it needs 22 guilds at or
// above `minimum` or the whole action fails; the fixture seeds 23.
struct legacy_guilds_mock_tester : guilds_fixture_base {
   std::vector<name> legacy_producers;   // defproducera..defproducerw, 23 registered producers

   legacy_guilds_mock_tester() : guilds_fixture_base( contracts::util::guild_test_wasm(), contracts::util::guild_test_abi() ) {
      std::vector<name> extra;
      for( char c = 'a'; c <= 'w'; ++c ) {
         name p( std::string("defproducer") + c );
         legacy_producers.push_back( p );
         if( std::find( producers.begin(), producers.end(), p ) == producers.end() ) extra.push_back( p );
      }
      setup_producer_accounts( extra );
      for( const auto& p : extra ) BOOST_REQUIRE_EQUAL( success(), regproducer( p ) );

      // apply() -> vote() sends eosio::voteproducer as top21.oig@active, so that account must exist,
      // carry guilds.oig@eosio.code on its active permission, and have voting weight
      const name top21 = "top21.oig"_n;
      create_account_with_resources( top21, config::system_account_name, core_sym::from_string("10.0000"), false,
                                     core_sym::from_string("100.0000"), core_sym::from_string("100.0000") );
      transfer( config::system_account_name, top21, core_sym::from_string("10000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL( success(), stake( top21, core_sym::from_string("1000.0000"), core_sym::from_string("1000.0000") ) );
      set_authority( top21, config::active_name,
                     authority( 1, { key_weight{ get_public_key( top21, "active" ), 1 } },
                                   { permission_level_weight{ { guilds_account, config::eosio_code_name }, 1 } } ),
                     config::owner_name );
      produce_blocks( 1 );
   }

   // the deployed-era header: the contract checks minimum > 10^decimals before anything else
   action_result legacy_pusheval( const name& signer, uint8_t type, const score_list& scores ) {
      return push_guild_action( signer, "pusheval"_n, mvo()
         ("minimum", 2)("decimals", 0)("scaling_threshhold", 1.0)("type", type)("scores", scores_variant( scores )) );
   }
   action_result legacy_setstate( const name& signer ) {
      return push_guild_action( signer, "setstate"_n, mvo()("minimum", 2)("decimals", 0)("scaling_threshhold", 1.0) );
   }
   action_result legacy_addguild( const name& signer, const name& producer, uint32_t score ) {
      return push_guild_action( signer, "addguild"_n, mvo()
         ("producer", producer)("score", score)
         ("target", time_point::from_iso_string( "2035-12-18T14:18:38" ))
         ("eligibility", core_sym::from_string("1.0000")) );
   }
};

} // namespace wcap_guilds

using namespace wcap_guilds;

BOOST_AUTO_TEST_SUITE(eosio_guilds_tests)

// ---------------------------------------------------------------------------------------------------
// fixture
// ---------------------------------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE( wcap_gld_fixture_pins_audited_build, eosio_guilds_tester ) try {
   // the bytes we loaded are the audited build, and they are what the chain is running
   BOOST_REQUIRE_EQUAL( guilds_718903f_sha256, wasm_sha256() );
   BOOST_REQUIRE_EQUAL( guilds_718903f_sha256, on_chain_code_hash() );
   BOOST_REQUIRE_EQUAL( guilds_718903f_abi_sha256, abi_sha256() );

   // 718903f is scoring-only: the legacy payment/eligibility surface is gone from the ABI
   const std::set<std::string> scoring_only_actions = { "pusheval", "retireguild", "rmguild" };
   BOOST_REQUIRE( abi_action_names() == scoring_only_actions );

   // clean slate, and the base tester's producers are registered for the addguild path
   BOOST_REQUIRE_EQUAL( 0u, guild_count() );
   BOOST_REQUIRE_EQUAL( 0u, evaluation_count() );
   for( const auto& p : producers ) BOOST_REQUIRE_EQUAL( true, get_producer_info( p )["is_active"].as<bool>() );
   BOOST_REQUIRE( get_row_by_account( config::system_account_name, config::system_account_name, "producers"_n, alice ).empty() );
} FC_LOG_AND_RETHROW()

// ---------------------------------------------------------------------------------------------------
// authorisation matrix (GLD-005 / SYS-008): every mutating action is gated on has_auth(oig)
// ---------------------------------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE( wcap_gld_auth_matrix_oig_only, eosio_guilds_tester ) try {
   // preconditions: A exists (for retireguild), B exists and is retired (for rmguild)
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10}, {prod_b, 20} } ) );
   BOOST_REQUIRE_EQUAL( success(), retireguild( oig, { prod_b } ) );
   const auto before = guild_snapshot( { prod_a, prod_b, prod_c } );
   const auto evals_before = evaluation_count();

   // (a) the contract account itself, (b) unrelated accounts, (c) a registered producer, (d) eosio
   for( const auto& signer : { guilds_account, treasury, alice, prod_c, config::system_account_name } ) {
      BOOST_TEST_MESSAGE( "signer " << signer.to_string() );
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( lacking_oig_authority ), pusheval( signer, 0, { {prod_a, 5} } ) );
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( lacking_oig_authority ), pusheval( signer, 0, { {prod_c, 5} } ) );
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( lacking_oig_authority ), retireguild( signer, { prod_a } ) );
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( lacking_oig_authority ), rmguild( signer, { prod_b } ) );
      require_unchanged( before );
      BOOST_REQUIRE( !has_guild( prod_c ) );
      BOOST_REQUIRE_EQUAL( evals_before, evaluation_count() );
   }

   // a forged oig@active authorization line without oig's signature never reaches the contract
   {
      auto r = push_guild_action_as( { oig, config::active_name }, alice, "active", "pusheval"_n,
                                     mvo()("type", 0)("scores", scores_variant( { {prod_a, 5} } )) );
      BOOST_TEST_MESSAGE( "forged oig@active: " << r );
      BOOST_REQUIRE_MESSAGE( r.find( "but does not have signatures for it" ) != std::string::npos, r );
      require_unchanged( before );
      BOOST_REQUIRE_EQUAL( evals_before, evaluation_count() );
   }

   // the same calls signed by oig@active succeed
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 5} } ) );
   BOOST_REQUIRE_EQUAL( 5u, score( prod_a ) );
   BOOST_REQUIRE_EQUAL( success(), retireguild( oig, { prod_a } ) );
   BOOST_REQUIRE_EQUAL( true, retired( prod_a ) );
   BOOST_REQUIRE_EQUAL( success(), rmguild( oig, { prod_b } ) );
   BOOST_REQUIRE( !has_guild( prod_b ) );
   BOOST_REQUIRE_EQUAL( evals_before + 1, evaluation_count() );

   // documentation (SYS-009): has_auth(oig) is satisfied by any oig permission, including oig@owner
   BOOST_REQUIRE_EQUAL( success(), push_guild_action_as( { oig, config::owner_name }, oig, "owner", "pusheval"_n,
                                                         mvo()("type", 0)("scores", scores_variant( { {prod_c, 7} } )) ) );
   BOOST_REQUIRE_EQUAL( 7u, score( prod_c ) );
   BOOST_REQUIRE_EQUAL( evals_before + 2, evaluation_count() );
} FC_LOG_AND_RETHROW()

// ---------------------------------------------------------------------------------------------------
// pusheval
// ---------------------------------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE( wcap_gld_008_empty_pusheval_is_refused, eosio_guilds_tester ) try {
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10}, {prod_b, 20}, {prod_c, 30} } ) );
   const auto before = guild_snapshot( producers );
   const auto evals_before = evaluation_count();

   for( uint8_t type : { 0, 1, 4 } ) {
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( empty_evaluation ), pusheval( oig, type, {} ) );
   }
   require_unchanged( before );                       // every row byte-identical, scores included
   BOOST_REQUIRE_EQUAL( evals_before, evaluation_count() );
   BOOST_REQUIRE_EQUAL( 10u, score( prod_a ) );
   BOOST_REQUIRE_EQUAL( 20u, score( prod_b ) );
   BOOST_REQUIRE_EQUAL( 30u, score( prod_c ) );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_gld_004_score_ceiling, eosio_guilds_tester ) try {
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, max_score} } ) );
   BOOST_REQUIRE_EQUAL( max_score, score( prod_a ) );
   const auto before = guild_snapshot( producers );
   const auto evals_before = evaluation_count();

   // one above the ceiling, existing row
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( score_ceiling ), pusheval( oig, 0, { {prod_a, max_score + 1} } ) );
   // one above the ceiling, new row
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( score_ceiling ), pusheval( oig, 0, { {prod_b, max_score + 1} } ) );
   // a later in-range duplicate does not rescue the out-of-range entry
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( score_ceiling ), pusheval( oig, 0, { {prod_a, max_score + 1}, {prod_a, 5} } ) );
   // a valid entry ahead of the bad one is rolled back with it
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( score_ceiling ), pusheval( oig, 0, { {prod_b, 3}, {prod_a, max_score + 1} } ) );
   // uint32 maximum
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( score_ceiling ), pusheval( oig, 0, { {prod_a, std::numeric_limits<uint32_t>::max()} } ) );

   require_unchanged( before );
   BOOST_REQUIRE( !has_guild( prod_b ) );
   BOOST_REQUIRE_EQUAL( evals_before, evaluation_count() );

   // zero is a valid score
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 0} } ) );
   BOOST_REQUIRE_EQUAL( 0u, score( prod_a ) );
   BOOST_REQUIRE_EQUAL( max_score, prv_score( prod_a ) );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_gld_addguild_requires_producer_row, eosio_guilds_tester ) try {
   const auto evals_before   = evaluation_count();
   const auto guilds_ram     = ram_usage( guilds_account );
   const auto oig_ram        = ram_usage( oig );

   // an existing account that is not a registered producer, and a name that is not an account at all
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( not_a_producer ), pusheval( oig, 0, { {alice, 10} } ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( not_a_producer ), pusheval( oig, 0, { {"zzz"_n, 10} } ) );
   BOOST_REQUIRE( !has_guild( alice ) );
   BOOST_REQUIRE( !has_guild( "zzz"_n ) );
   BOOST_REQUIRE_EQUAL( evals_before, evaluation_count() );

   // one bad element in the middle rolls back the good ones around it
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( not_a_producer ), pusheval( oig, 0, { {prod_a, 1}, {alice, 2}, {prod_b, 3} } ) );
   BOOST_REQUIRE_EQUAL( 0u, guild_count() );
   BOOST_REQUIRE_EQUAL( evals_before, evaluation_count() );
   BOOST_REQUIRE_EQUAL( guilds_ram, ram_usage( guilds_account ) );

   // a registered producer gets a row with the documented defaults, RAM paid by the contract
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_d, 10} } ) );
   auto g = guild( prod_d );
   BOOST_REQUIRE_EQUAL( prod_d, g["producer"].as<name>() );
   BOOST_REQUIRE_EQUAL( 10u, g["score"].as<uint32_t>() );
   BOOST_REQUIRE_EQUAL( 0u, g["prv_score"].as<uint32_t>() );
   BOOST_REQUIRE_EQUAL( "0.00000000 WAX", g["balance"].as_string() );
   BOOST_REQUIRE_EQUAL( "0.00000000 WAX", g["eligibility"].as_string() );
   BOOST_REQUIRE_EQUAL( false, g["autopay"].as<bool>() );
   BOOST_REQUIRE_EQUAL( false, g["retired"].as<bool>() );
   BOOST_REQUIRE_GT( ram_usage( guilds_account ), guilds_ram );
   BOOST_REQUIRE_EQUAL( oig_ram, ram_usage( oig ) );
   BOOST_REQUIRE_EQUAL( evals_before + 1, evaluation_count() );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_gld_i6_omitted_guilds_are_untouched, eosio_guilds_tester ) try {
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10}, {prod_b, 20}, {prod_c, 30} } ) );
   BOOST_REQUIRE_EQUAL( success(), retireguild( oig, { prod_c } ) );
   const auto before = guild_snapshot( { prod_b, prod_c } );

   // omission is not ejection: only the submitted guild moves
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 11} } ) );
   BOOST_REQUIRE_EQUAL( 11u, score( prod_a ) );
   BOOST_REQUIRE_EQUAL( 10u, prv_score( prod_a ) );
   BOOST_REQUIRE_EQUAL( false, retired( prod_a ) );
   require_unchanged( before );
   BOOST_REQUIRE_EQUAL( 20u, score( prod_b ) );
   BOOST_REQUIRE_EQUAL( 0u,  prv_score( prod_b ) );
   BOOST_REQUIRE_EQUAL( true, retired( prod_c ) );   // a retired, omitted guild stays retired

   // a second push snapshots the previous push's score
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 1, { {prod_a, 12} } ) );
   BOOST_REQUIRE_EQUAL( 12u, score( prod_a ) );
   BOOST_REQUIRE_EQUAL( 11u, prv_score( prod_a ) );
   require_unchanged( before );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_gld_pusheval_type_is_a_label, eosio_guilds_tester ) try {
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10}, {prod_b, 20} } ) );
   const auto before = guild_snapshot( { prod_b } );

   // 0 and 1 are the documented labels; 4 exists in the live history; nothing restricts the value
   uint32_t expected = 10;
   for( uint8_t type : { 0, 1, 4, 255 } ) {
      BOOST_REQUIRE_EQUAL( success(), pusheval( oig, type, { {prod_a, expected + 1} } ) );
      BOOST_REQUIRE_EQUAL( expected + 1, score( prod_a ) );
      BOOST_REQUIRE_EQUAL( expected,     prv_score( prod_a ) );
      require_unchanged( before );
      const auto evals = evaluations();
      BOOST_REQUIRE_EQUAL( unsigned(type), unsigned( evals.back()["type"].as<uint8_t>() ) );
      BOOST_REQUIRE_EQUAL( 0u,             evals.back()["minimum"].as<uint32_t>() );
      BOOST_REQUIRE_EQUAL( 0u,             unsigned( evals.back()["decimals"].as<uint8_t>() ) );
      ++expected;
   }
   BOOST_REQUIRE_EQUAL( 5u, evaluation_count() );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_gld_pusheval_duplicates_last_wins_snapshot_kept, eosio_guilds_tester ) try {
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10} } ) );

   // existing guild: last entry wins, prv_score is the pre-push score (not the first duplicate)
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 1}, {prod_a, 2} } ) );
   BOOST_REQUIRE_EQUAL( 2u,  score( prod_a ) );
   BOOST_REQUIRE_EQUAL( 10u, prv_score( prod_a ) );

   // new guild via duplicates: created by the first entry, overwritten by the second, prv_score stays 0
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_d, 1}, {prod_d, 2} } ) );
   BOOST_REQUIRE_EQUAL( 2u, score( prod_d ) );
   BOOST_REQUIRE_EQUAL( 0u, prv_score( prod_d ) );

   // the evaluation row keeps the submitted vector verbatim, duplicates included
   const auto evals = evaluations();
   BOOST_REQUIRE_EQUAL( 3u, evals.size() );
   BOOST_REQUIRE_EQUAL( 2u, evals.back()["scores"].get_array().size() );
   BOOST_REQUIRE_EQUAL( prod_d, evals.back()["scores"].get_array()[0]["guild"].as<name>() );
   BOOST_REQUIRE_EQUAL( 1u,     evals.back()["scores"].get_array()[0]["score"].as<uint32_t>() );
   BOOST_REQUIRE_EQUAL( 2u,     evals.back()["scores"].get_array()[1]["score"].as<uint32_t>() );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_gld_pusheval_reactivates_retired, eosio_guilds_tester ) try {
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10} } ) );
   BOOST_REQUIRE_EQUAL( success(), retireguild( oig, { prod_a } ) );
   BOOST_REQUIRE_EQUAL( true, retired( prod_a ) );

   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 7} } ) );
   BOOST_REQUIRE_EQUAL( false, retired( prod_a ) );
   BOOST_REQUIRE_EQUAL( 7u, score( prod_a ) );
   BOOST_REQUIRE_EQUAL( 0u, prv_score( prod_a ) );   // zeroed by retire, then snapshotted
} FC_LOG_AND_RETHROW()

// ---------------------------------------------------------------------------------------------------
// retireguild / rmguild
// ---------------------------------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE( wcap_gld_retire_zeroes_score, eosio_guilds_tester ) try {
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10}, {prod_b, 20} } ) );
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 1, { {prod_a, 12} } ) );
   BOOST_REQUIRE_EQUAL( 10u, prv_score( prod_a ) );
   const auto before = guild_snapshot( { prod_b } );
   const auto evals_before = evaluation_count();

   BOOST_REQUIRE_EQUAL( success(), retireguild( oig, { prod_a } ) );
   auto g = guild( prod_a );
   BOOST_REQUIRE_EQUAL( 0u,   g["score"].as<uint32_t>() );
   BOOST_REQUIRE_EQUAL( 0u,   g["prv_score"].as<uint32_t>() );
   BOOST_REQUIRE_EQUAL( true, g["retired"].as<bool>() );
   // the legacy money fields keep their defaults; nothing at 718903f can set balance or eligibility
   BOOST_REQUIRE_EQUAL( "0.00000000 WAX", g["balance"].as_string() );
   BOOST_REQUIRE_EQUAL( "0.00000000 WAX", g["eligibility"].as_string() );
   BOOST_REQUIRE_EQUAL( false, g["autopay"].as<bool>() );
   require_unchanged( before );
   BOOST_REQUIRE_EQUAL( evals_before, evaluation_count() );

   // already retired is accepted; the empty vector is a no-op
   BOOST_REQUIRE_EQUAL( success(), retireguild( oig, { prod_a } ) );
   BOOST_REQUIRE_EQUAL( success(), retireguild( oig, {} ) );
   require_unchanged( before );
   BOOST_REQUIRE_EQUAL( 2u, guild_count() );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_gld_retire_unknown_rolls_back, eosio_guilds_tester ) try {
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10} } ) );
   const auto before = guild_snapshot( { prod_a } );

   // the loop consumes the vector back to front, so the good entry is processed first in the second call
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( unknown_guild ), retireguild( oig, { prod_a, "zzz"_n } ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( unknown_guild ), retireguild( oig, { "zzz"_n, prod_a } ) );
   // a registered producer without a guilds row is unknown too
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( unknown_guild ), retireguild( oig, { prod_b, prod_a } ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( unknown_guild ), retireguild( oig, { alice } ) );
   require_unchanged( before );
   BOOST_REQUIRE_EQUAL( false, retired( prod_a ) );
   BOOST_REQUIRE_EQUAL( 10u, score( prod_a ) );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_gld_rm_requires_retired, eosio_guilds_tester ) try {
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10}, {prod_b, 20} } ) );
   const auto evals_before = evaluation_count();

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( guild_not_retired ), rmguild( oig, { prod_a } ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( unknown_guild ),     rmguild( oig, { "zzz"_n } ) );
   BOOST_REQUIRE_EQUAL( success(), rmguild( oig, {} ) );
   BOOST_REQUIRE_EQUAL( 2u, guild_count() );

   BOOST_REQUIRE_EQUAL( success(), retireguild( oig, { prod_a } ) );
   const auto before = guild_snapshot( { prod_a, prod_b } );

   // duplicate: the second copy finds no row, and the first erase is rolled back with it
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( unknown_guild ), rmguild( oig, { prod_a, prod_a } ) );
   require_unchanged( before );
   // back-to-front: A (retired) is erased first, then B (active) fails and A comes back
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( guild_not_retired ), rmguild( oig, { prod_b, prod_a } ) );
   require_unchanged( before );

   BOOST_REQUIRE_EQUAL( success(), rmguild( oig, { prod_a } ) );
   BOOST_REQUIRE( !has_guild( prod_a ) );
   BOOST_REQUIRE_EQUAL( 1u, guild_count() );
   BOOST_REQUIRE_EQUAL( evals_before, evaluation_count() );   // history survives retire and remove

   // a removed guild can be scored back in as a fresh row
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 3} } ) );
   BOOST_REQUIRE_EQUAL( 3u, score( prod_a ) );
   BOOST_REQUIRE_EQUAL( 0u, prv_score( prod_a ) );
} FC_LOG_AND_RETHROW()

// ---------------------------------------------------------------------------------------------------
// evaluations key (GLD-003, open at 718903f, tracked on WBP-2017)
// ---------------------------------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE( wcap_gld_003_same_block_pusheval_collides, eosio_guilds_tester ) try {
   // evaluation::primary_key() is the block time, so two pusheval in one transaction (or one block)
   // emplace the same key. The second db_store_i64 fails inside chainbase
   // (libraries/chaindb/include/chainbase/undo_index.hpp:365, a std::logic_error rethrown by the chain),
   // not in the contract: there is no eosio_assert, so the message carries no "assertion failure with
   // message:" prefix and the transaction's own context is appended. This pins the current behaviour;
   // WBP-2017 tracks the fix. INVERT ON FIX: the two-action transaction succeeds with two rows, i.e.
   //    push_transaction( trx ); produce_block();
   //    BOOST_REQUIRE_EQUAL( evals_before + 2, evaluation_count() );
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 0, { {prod_a, 10} } ) );
   const auto before = guild_snapshot( { prod_a, prod_b } );
   const auto evals_before = evaluation_count();

   auto two_pushes_one_transaction = [&]() {
      signed_transaction trx;
      for( const auto& s : { score_list{ {prod_a, 11} }, score_list{ {prod_b, 20} } } ) {
         action act = make_guild_action( "pusheval"_n, mvo()("type", 0)("scores", scores_variant( s )) );
         act.authorization = { { oig, config::active_name } };
         trx.actions.emplace_back( std::move(act) );
      }
      set_transaction_headers( trx );
      trx.sign( get_private_key( oig, "active" ), control->get_chain_id() );
      return trx;
   };

   const std::string duplicate_key = error( "could not insert object, most likely a uniqueness constraint was violated"
                                            ": guilds.oig <= guilds.oig::pusheval pending console output: " );

   // one transaction, two actions
   {
      std::string msg;
      auto trx = two_pushes_one_transaction();
      try { push_transaction( trx ); }
      catch( const fc::exception& ex ) { msg = ex.top_message(); }
      BOOST_REQUIRE_EQUAL( duplicate_key, msg );
      require_unchanged( before );
      BOOST_REQUIRE( !has_guild( prod_b ) );
      BOOST_REQUIRE_EQUAL( evals_before, evaluation_count() );
   }

   // two transactions in the same block collide the same way
   {
      auto first = make_guild_action( "pusheval"_n, mvo()("type", 0)("scores", scores_variant( { {prod_a, 11} } )) );
      first.authorization = { { oig, config::active_name } };
      signed_transaction trx1;
      trx1.actions.emplace_back( std::move(first) );
      set_transaction_headers( trx1 );
      trx1.sign( get_private_key( oig, "active" ), control->get_chain_id() );
      push_transaction( trx1 );

      auto second = make_guild_action( "pusheval"_n, mvo()("type", 1)("scores", scores_variant( { {prod_b, 20} } )) );
      second.authorization = { { oig, config::active_name } };
      signed_transaction trx2;
      trx2.actions.emplace_back( std::move(second) );
      set_transaction_headers( trx2 );
      trx2.sign( get_private_key( oig, "active" ), control->get_chain_id() );
      std::string msg;
      try { push_transaction( trx2 ); }
      catch( const fc::exception& ex ) { msg = ex.top_message(); }
      BOOST_REQUIRE_EQUAL( duplicate_key, msg );
      produce_block();
      BOOST_REQUIRE_EQUAL( true, chain_has_transaction( trx1.id() ) );
      BOOST_REQUIRE_EQUAL( 11u, score( prod_a ) );
      BOOST_REQUIRE( !has_guild( prod_b ) );
      BOOST_REQUIRE_EQUAL( evals_before + 1, evaluation_count() );
   }

   // the next block accepts the second push
   BOOST_REQUIRE_EQUAL( success(), pusheval( oig, 1, { {prod_b, 20} } ) );
   BOOST_REQUIRE_EQUAL( 20u, score( prod_b ) );
   BOOST_REQUIRE_EQUAL( evals_before + 2, evaluation_count() );
} FC_LOG_AND_RETHROW()

// ---------------------------------------------------------------------------------------------------
// GLD-008 on the legacy mock: the deployed-era shape ejects every guild the vector omits
// ---------------------------------------------------------------------------------------------------
//
// The plan asked for "empty push zeroes every score" on the mock. It cannot be shown there: pusheval's
// clearscores() does zero every non-retired score, but the unconditional apply() that follows (roster
// requirement: see legacy_guilds_mock_tester) finds nothing at or above `minimum` (>= 2 by the header
// check), and the whole action rolls back with its message. The deployed 6085791 has the same sequence
// (src/guilds_oig.cpp 314, 323, 286, 584). The empty-vector defect is real only where apply() is gone,
// i.e. the 4d8e2d0 refactor, and 718903f closes it (wcap_gld_008_empty_pusheval_is_refused above).
// What the deployed-era shape does exhibit, and 718903f changes, is that omission is ejection.

BOOST_FIXTURE_TEST_CASE( wcap_gld_008_legacy_mock_omission_is_ejection, legacy_guilds_mock_tester ) try {
   BOOST_REQUIRE( abi_action_names().count( "addguild" ) == 1 );   // the legacy surface, not 718903f
   BOOST_REQUIRE_EQUAL( success(), legacy_setstate( guilds_account ) );

   uint32_t s = 100;
   for( const auto& p : legacy_producers ) BOOST_REQUIRE_EQUAL( success(), legacy_addguild( guilds_account, p, s++ ) );
   BOOST_REQUIRE_EQUAL( 23u, guild_count() );
   const auto before = guild_snapshot( legacy_producers );

   // the empty push is refused, but by apply()'s roster check after clearscores() already ran, not by design
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "Less than 21 guilds passing minimum score." ), legacy_pusheval( guilds_account, 0, {} ) );
   require_unchanged( before );
   BOOST_REQUIRE_EQUAL( 0u, evaluation_count() );

   // a push naming 22 of the 23 guilds: the omitted one is zeroed (score -> prv_score) and stays non-retired
   score_list twenty_two;
   for( size_t i = 0; i + 1 < legacy_producers.size(); ++i ) twenty_two.emplace_back( legacy_producers[i], 1000 + i );
   const name omitted = legacy_producers.back();
   BOOST_REQUIRE_EQUAL( 122u, score( omitted ) );
   BOOST_REQUIRE_EQUAL( success(), legacy_pusheval( guilds_account, 0, twenty_two ) );
   BOOST_REQUIRE_EQUAL( 0u,    score( omitted ) );
   BOOST_REQUIRE_EQUAL( 122u,  prv_score( omitted ) );
   BOOST_REQUIRE_EQUAL( false, retired( omitted ) );
   for( size_t i = 0; i + 1 < legacy_producers.size(); ++i ) {
      BOOST_REQUIRE_EQUAL( 1000u + i, score( legacy_producers[i] ) );
      BOOST_REQUIRE_EQUAL( 100u + i,  prv_score( legacy_producers[i] ) );
   }
   BOOST_REQUIRE_EQUAL( 1u, evaluation_count() );
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
