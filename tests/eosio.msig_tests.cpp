#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/wast_to_wasm.hpp>

#include <fc/variant_object.hpp>
#include "contracts.hpp"
#include "test_symbol.hpp"

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;

class eosio_msig_tester : public tester {
public:
   eosio_msig_tester() {
      create_accounts( { "eosio.msig"_n, "eosio.stake"_n, "eosio.ram"_n, "eosio.ramfee"_n, "alice"_n, "bob"_n, "carol"_n } );
      produce_block();

      auto trace = base_tester::push_action(config::system_account_name, "setpriv"_n,
                                            config::system_account_name,  mutable_variant_object()
                                            ("account", "eosio.msig")
                                            ("is_priv", 1)
      );

      set_code( "eosio.msig"_n, contracts::msig_wasm() );
      set_abi( "eosio.msig"_n, contracts::msig_abi().data() );

      produce_blocks();
      const auto& accnt = control->db().get<account_object,by_name>( "eosio.msig"_n );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
   }

   transaction_trace_ptr create_account_with_resources( account_name a, account_name creator, asset ramfunds, bool multisig,
                                                        asset net = core_sym::from_string("10.0000"), asset cpu = core_sym::from_string("10.0000") ) {
      signed_transaction trx;
      set_transaction_headers(trx);

      authority owner_auth;
      if (multisig) {
         // multisig between account's owner key and creators active permission
         owner_auth = authority(2, {key_weight{get_public_key( a, "owner" ), 1}}, {permission_level_weight{{creator, config::active_name}, 1}});
      } else {
         owner_auth =  authority( get_public_key( a, "owner" ) );
      }

      trx.actions.emplace_back( vector<permission_level>{{creator,config::active_name}},
                                newaccount{
                                   .creator  = creator,
                                   .name     = a,
                                   .owner    = owner_auth,
                                   .active   = authority( get_public_key( a, "active" ) )
                                });

      trx.actions.emplace_back( get_action( "eosio"_n, "buyram"_n, vector<permission_level>{{creator,config::active_name}},
                                            mvo()
                                            ("payer", creator)
                                            ("receiver", a)
                                            ("quant", ramfunds) )
                              );

      trx.actions.emplace_back( get_action( "eosio"_n, "delegatebw"_n, vector<permission_level>{{creator,config::active_name}},
                                            mvo()
                                            ("from", creator)
                                            ("receiver", a)
                                            ("stake_net_quantity", net )
                                            ("stake_cpu_quantity", cpu )
                                            ("transfer", 0 )
                                          )
                                );

      set_transaction_headers(trx);
      trx.sign( get_private_key( creator, "active" ), control->get_chain_id()  );
      return push_transaction( trx );
   }
   void create_currency( name contract, name manager, asset maxsupply ) {
      auto act =  mutable_variant_object()
         ("issuer",       manager )
         ("maximum_supply", maxsupply );

      base_tester::push_action(contract, "create"_n, contract, act );
   }
   void issue( name to, const asset& amount, name manager = config::system_account_name ) {
      base_tester::push_action( "eosio.token"_n, "issue"_n, manager, mutable_variant_object()
                                ("to",      to )
                                ("quantity", amount )
                                ("memo", "")
                                );
   }
   void transfer( name from, name to, const string& amount, name manager = config::system_account_name ) {
      base_tester::push_action( "eosio.token"_n, "transfer"_n, manager, mutable_variant_object()
                                ("from",    from)
                                ("to",      to )
                                ("quantity", asset::from_string(amount) )
                                ("memo", "")
                                );
   }
   asset get_balance( const account_name& act ) {
      //return get_currency_balance( config::system_account_name, symbol(CORE_SYMBOL), act );
      //temporary code. current get_currency_balancy uses table name "accounts"_n from currency.h
      //generic_currency table name is "account"_n.
      const auto& db  = control->db();
      const auto* tbl = db.find<table_id_object, by_code_scope_table>(boost::make_tuple("eosio.token"_n, act, "accounts"_n));
      share_type result = 0;

      // the balance is implied to be 0 if either the table or row does not exist
      if (tbl) {
         const auto *obj = db.find<key_value_object, by_scope_primary>(boost::make_tuple(tbl->id, symbol(CORE_SYM).to_symbol_code()));
         if (obj) {
            // balance is the first field in the serialization
            fc::datastream<const char *> ds(obj->value.data(), obj->value.size());
            fc::raw::unpack(ds, result);
         }
      }
      return asset( result, symbol(CORE_SYM) );
   }

   transaction_trace_ptr push_action( const account_name& signer, const action_name& name, const variant_object& data, bool auth = true ) {
      vector<account_name> accounts;
      if( auth )
         accounts.push_back( signer );
      auto trace = base_tester::push_action( "eosio.msig"_n, name, accounts, data );
      produce_block();
      BOOST_REQUIRE_EQUAL( true, chain_has_transaction(trace->id) );
      return trace;

      /*
         string action_type_name = abi_ser.get_action_type(name);

         action act;
         act.account = "eosio.msig"_n;
         act.name = name;
         act.data = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );
         //std::cout << "test:\n" << fc::to_hex(act.data.data(), act.data.size()) << " size = " << act.data.size() << std::endl;

         return base_tester::push_action( std::move(act), auth ? uint64_t(signer) : 0 );
      */
   }

   transaction reqauth( account_name from, const vector<permission_level>& auths, const fc::microseconds& max_serialization_time );

   // Pushes `propose` with the action data assembled by hand, so the proposed transaction is exactly
   // `packed_trx` rather than whatever the ABI serializer would produce from a variant.
   transaction_trace_ptr propose_raw( name proposer, name proposal_name, const vector<permission_level>& requested, const bytes& packed_trx ) {
      action act;
      act.account       = "eosio.msig"_n;
      act.name          = "propose"_n;
      act.authorization = { permission_level{ proposer, config::active_name } };
      for( const bytes& part : { fc::raw::pack( proposer ), fc::raw::pack( proposal_name ), fc::raw::pack( requested ), packed_trx } ) {
         act.data.insert( act.data.end(), part.begin(), part.end() );
      }

      signed_transaction trx;
      trx.actions.push_back( std::move(act) );
      set_transaction_headers( trx );
      trx.sign( get_private_key( proposer, "active" ), control->get_chain_id() );
      auto trace = push_transaction( trx );
      produce_block();
      return trace;
   }

   abi_serializer abi_ser;
};

transaction eosio_msig_tester::reqauth( account_name from, const vector<permission_level>& auths, const fc::microseconds& max_serialization_time ) {
   fc::variants v;
   for ( auto& level : auths ) {
      v.push_back(fc::mutable_variant_object()
                  ("actor", level.actor)
                  ("permission", level.permission)
      );
   }
   fc::variant pretty_trx = fc::mutable_variant_object()
      ("expiration", "2020-01-01T00:30")
      ("ref_block_num", 2)
      ("ref_block_prefix", 3)
      ("max_net_usage_words", 0)
      ("max_cpu_usage_ms", 0)
      ("delay_sec", 0)
      ("actions", fc::variants({
            fc::mutable_variant_object()
               ("account", name(config::system_account_name))
               ("name", "reqauth")
               ("authorization", v)
               ("data", fc::mutable_variant_object() ("from", from) )
               })
      );
   transaction trx;
   abi_serializer::from_variant(pretty_trx, trx, get_resolver(), abi_serializer::create_yield_function(max_serialization_time));
   return trx;
}

BOOST_AUTO_TEST_SUITE(eosio_msig_tests)

BOOST_FIXTURE_TEST_CASE( propose_approve_execute, eosio_msig_tester ) try {
   auto trx = reqauth( "alice"_n, {permission_level{"alice"_n, config::active_name}}, abi_serializer_max_time );

   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", vector<permission_level>{{ "alice"_n, config::active_name }})
   );

   //fail to execute before approval
   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "exec"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("executer",      "alice")
                            ),
                            eosio_assert_message_exception,
                            eosio_assert_message_is("transaction authorization failed")
   );

   //approve and execute
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   auto r = push_action( "alice"_n, "exec"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("executer",      "alice")
   );

   BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
   BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( propose_approve_unapprove, eosio_msig_tester ) try {
   auto trx = reqauth( "alice"_n, {permission_level{"alice"_n, config::active_name}}, abi_serializer_max_time );

   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", vector<permission_level>{{ "alice"_n, config::active_name }})
   );

   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   push_action( "alice"_n, "unapprove"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "exec"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("executer",      "alice")
                            ),
                            eosio_assert_message_exception,
                            eosio_assert_message_is("transaction authorization failed")
   );

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( propose_approve_by_two, eosio_msig_tester ) try {
   auto trx = reqauth( "alice"_n, vector<permission_level>{ { "alice"_n, config::active_name }, { "bob"_n, config::active_name } }, abi_serializer_max_time );
   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", vector<permission_level>{ { "alice"_n, config::active_name }, { "bob"_n, config::active_name } })
   );

   //approve by alice
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   //fail because approval by bob is missing

   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "exec"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("executer",      "alice")
                            ),
                            eosio_assert_message_exception,
                            eosio_assert_message_is("transaction authorization failed")
   );

   //approve by bob and execute
   push_action( "bob"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "bob"_n, config::active_name })
   );

   auto r = push_action( "alice"_n, "exec"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("executer",      "alice")
   );

   BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
   BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( propose_with_wrong_requested_auth, eosio_msig_tester ) try {
   auto trx = reqauth( "alice"_n, vector<permission_level>{ { "alice"_n, config::active_name },  { "bob"_n, config::active_name } }, abi_serializer_max_time );
   //try with not enough requested auth
   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "propose"_n, mvo()
                                             ("proposer",      "alice")
                                             ("proposal_name", "third")
                                             ("trx",           trx)
                                             ("requested", vector<permission_level>{ { "alice"_n, config::active_name } } )
                            ),
                            eosio_assert_message_exception,
                            eosio_assert_message_is("transaction authorization failed")
   );

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( big_transaction, eosio_msig_tester ) try {
   vector<permission_level> perm = { { "alice"_n, config::active_name }, { "bob"_n, config::active_name } };
   auto wasm = contracts::util::exchange_wasm();

   fc::variant pretty_trx = fc::mutable_variant_object()
      ("expiration", "2020-01-01T00:30")
      ("ref_block_num", 2)
      ("ref_block_prefix", 3)
      ("max_net_usage_words", 0)
      ("max_cpu_usage_ms", 0)
      ("delay_sec", 0)
      ("actions", fc::variants({
            fc::mutable_variant_object()
               ("account", name(config::system_account_name))
               ("name", "setcode")
               ("authorization", perm)
               ("data", fc::mutable_variant_object()
                ("account", "alice")
                ("vmtype", 0)
                ("vmversion", 0)
                ("code", bytes( wasm.begin(), wasm.end() ))
               )
               })
      );

   transaction trx;
   abi_serializer::from_variant(pretty_trx, trx, get_resolver(), abi_serializer::create_yield_function(abi_serializer_max_time));

   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", perm)
   );

   //approve by alice
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );
   //approve by bob and execute
   push_action( "bob"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "bob"_n, config::active_name })
   );

   auto r = push_action( "alice"_n, "exec"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("executer",      "alice")
   );

   BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
   BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );
} FC_LOG_AND_RETHROW()



BOOST_FIXTURE_TEST_CASE( update_system_contract_all_approve, eosio_msig_tester ) try {

   // required to set up the link between (eosio active) and (eosio.prods active)
   //
   //                  eosio active
   //                       |
   //             eosio.prods active (2/3 threshold)
   //             /         |        \             <--- implicitly updated in onblock action
   // alice active     bob active   carol active

   set_authority(
      config::system_account_name,
      config::active_name,
      authority( 1,
                 vector<key_weight>{{get_private_key(config::system_account_name, "active").get_public_key(), 1}},
                 vector<permission_level_weight>{{{"eosio.prods"_n, config::active_name}, 1}}
      ),
      config::owner_name,
      {{config::system_account_name, config::active_name}},
      {get_private_key(config::system_account_name, "active")}
   );

   set_producers( {"alice"_n,"bob"_n,"carol"_n} );
   produce_blocks(50);

   create_accounts( { "eosio.token"_n, "eosio.rex"_n } );
   set_code( "eosio.token"_n, contracts::token_wasm() );
   set_abi( "eosio.token"_n, contracts::token_abi().data() );

   create_currency( "eosio.token"_n, config::system_account_name, core_sym::from_string("10000000000.0000") );
   issue(config::system_account_name, core_sym::from_string("1000000000.0000"));
   BOOST_REQUIRE_EQUAL( core_sym::from_string("1000000000.0000"),
                        get_balance(config::system_account_name) + get_balance("eosio.ramfee"_n) + get_balance("eosio.stake"_n) + get_balance("eosio.ram"_n) );

   set_code( config::system_account_name, contracts::system_wasm() );
   set_abi( config::system_account_name, contracts::system_abi().data() );
   base_tester::push_action( config::system_account_name, "init"_n,
                             config::system_account_name,  mutable_variant_object()
                              ("version", 0)
                              ("core", CORE_SYM_STR)
   );
   produce_blocks();
   create_account_with_resources( "alice1111111"_n, "eosio"_n, core_sym::from_string("1.0000"), false );
   create_account_with_resources( "bob111111111"_n, "eosio"_n, core_sym::from_string("0.4500"), false );
   create_account_with_resources( "carol1111111"_n, "eosio"_n, core_sym::from_string("1.0000"), false );

   BOOST_REQUIRE_EQUAL( core_sym::from_string("1000000000.0000"),
                        get_balance(config::system_account_name) + get_balance("eosio.ramfee"_n) + get_balance("eosio.stake"_n) + get_balance("eosio.ram"_n) );

   vector<permission_level> perm = { { "alice"_n, config::active_name }, { "bob"_n, config::active_name },
      {"carol"_n, config::active_name} };

   vector<permission_level> action_perm = {{"eosio"_n, config::active_name}};

   auto wasm = contracts::util::reject_all_wasm();

   fc::variant pretty_trx = fc::mutable_variant_object()
      ("expiration", "2020-01-01T00:30")
      ("ref_block_num", 2)
      ("ref_block_prefix", 3)
      ("max_net_usage_words", 0)
      ("max_cpu_usage_ms", 0)
      ("delay_sec", 0)
      ("actions", fc::variants({
            fc::mutable_variant_object()
               ("account", name(config::system_account_name))
               ("name", "setcode")
               ("authorization", action_perm)
               ("data", fc::mutable_variant_object()
                ("account", name(config::system_account_name))
                ("vmtype", 0)
                ("vmversion", 0)
                ("code", bytes( wasm.begin(), wasm.end() ))
               )
               })
      );

   transaction trx;
   abi_serializer::from_variant(pretty_trx, trx, get_resolver(), abi_serializer::create_yield_function(abi_serializer_max_time));

   // propose action
   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", perm)
   );

   //approve by alice
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );
   //approve by bob
   push_action( "bob"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "bob"_n, config::active_name })
   );
   //approve by carol
   push_action( "carol"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "carol"_n, config::active_name })
   );
   // execute by alice to replace the eosio system contract

   auto r = push_action( "alice"_n, "exec"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("executer",      "alice")
   );

   BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
   BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );

   // can't create account because system contract was replaced by the reject_all contract

   BOOST_REQUIRE_EXCEPTION( create_account_with_resources( "alice1111112"_n, "eosio"_n, core_sym::from_string("1.0000"), false ),
                            eosio_assert_message_exception, eosio_assert_message_is("rejecting all actions")

   );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( update_system_contract_major_approve, eosio_msig_tester ) try {

   // set up the link between (eosio active) and (eosio.prods active)
   set_authority(
      config::system_account_name,
      config::active_name,
      authority( 1,
                 vector<key_weight>{{get_private_key(config::system_account_name, "active").get_public_key(), 1}},
                 vector<permission_level_weight>{{{"eosio.prods"_n, config::active_name}, 1}}
      ),
      config::owner_name,
      {{config::system_account_name, config::active_name}},
      {get_private_key(config::system_account_name, "active")}
   );

   create_accounts( { "apple"_n } );
   set_producers( {"alice"_n,"bob"_n,"carol"_n, "apple"_n} );
   produce_blocks(50);

   create_accounts( { "eosio.token"_n, "eosio.rex"_n } );
   set_code( "eosio.token"_n, contracts::token_wasm() );
   set_abi( "eosio.token"_n, contracts::token_abi().data() );

   create_currency( "eosio.token"_n, config::system_account_name, core_sym::from_string("10000000000.0000") );
   issue(config::system_account_name, core_sym::from_string("1000000000.0000"));
   BOOST_REQUIRE_EQUAL( core_sym::from_string("1000000000.0000"), get_balance( config::system_account_name ) );

   set_code( config::system_account_name, contracts::system_wasm() );
   set_abi( config::system_account_name, contracts::system_abi().data() );
   base_tester::push_action( config::system_account_name, "init"_n,
                             config::system_account_name,  mutable_variant_object()
                                 ("version", 0)
                                 ("core", CORE_SYM_STR)
   );
   produce_blocks();

   create_account_with_resources( "alice1111111"_n, "eosio"_n, core_sym::from_string("1.0000"), false );
   create_account_with_resources( "bob111111111"_n, "eosio"_n, core_sym::from_string("0.4500"), false );
   create_account_with_resources( "carol1111111"_n, "eosio"_n, core_sym::from_string("1.0000"), false );

   BOOST_REQUIRE_EQUAL( core_sym::from_string("1000000000.0000"),
                        get_balance(config::system_account_name) + get_balance("eosio.ramfee"_n) + get_balance("eosio.stake"_n) + get_balance("eosio.ram"_n) );

   vector<permission_level> perm = { { "alice"_n, config::active_name }, { "bob"_n, config::active_name },
      {"carol"_n, config::active_name}, {"apple"_n, config::active_name}};

   vector<permission_level> action_perm = {{"eosio"_n, config::active_name}};

   auto wasm = contracts::util::reject_all_wasm();

   fc::variant pretty_trx = fc::mutable_variant_object()
      ("expiration", "2020-01-01T00:30")
      ("ref_block_num", 2)
      ("ref_block_prefix", 3)
      ("max_net_usage_words", 0)
      ("max_cpu_usage_ms", 0)
      ("delay_sec", 0)
      ("actions", fc::variants({
            fc::mutable_variant_object()
               ("account", name(config::system_account_name))
               ("name", "setcode")
               ("authorization", action_perm)
               ("data", fc::mutable_variant_object()
                ("account", name(config::system_account_name))
                ("vmtype", 0)
                ("vmversion", 0)
                ("code", bytes( wasm.begin(), wasm.end() ))
               )
               })
      );

   transaction trx;
   abi_serializer::from_variant(pretty_trx, trx, get_resolver(), abi_serializer::create_yield_function(abi_serializer_max_time));

   // propose action
   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", perm)
   );

   //approve by alice
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );
   //approve by bob
   push_action( "bob"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "bob"_n, config::active_name })
   );

   // not enough approvers
   BOOST_REQUIRE_EXCEPTION(
      push_action( "alice"_n, "exec"_n, mvo()
                     ("proposer",      "alice")
                     ("proposal_name", "first")
                     ("executer",      "alice")
      ),
      eosio_assert_message_exception, eosio_assert_message_is("transaction authorization failed")
   );

   //approve by apple
   push_action( "apple"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "apple"_n, config::active_name })
   );
   // execute by alice to replace the eosio system contract

   // execute by another producer different from proposer
   auto r = push_action( "apple"_n, "exec"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("executer",      "apple")
   );

   BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
   BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );

   // can't create account because system contract was replaced by the reject_all contract

   BOOST_REQUIRE_EXCEPTION( create_account_with_resources( "alice1111112"_n, "eosio"_n, core_sym::from_string("1.0000"), false ),
                            eosio_assert_message_exception, eosio_assert_message_is("rejecting all actions")

   );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( propose_approve_invalidate, eosio_msig_tester ) try {
   auto trx = reqauth( "alice"_n, {permission_level{"alice"_n, config::active_name}}, abi_serializer_max_time );

   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", vector<permission_level>{{ "alice"_n, config::active_name }})
   );

   //fail to execute before approval
   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "exec"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("executer",      "alice")
                            ),
                            eosio_assert_message_exception,
                            eosio_assert_message_is("transaction authorization failed")
   );

   //approve
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   //invalidate
   push_action( "alice"_n, "invalidate"_n, mvo()
                  ("account",      "alice")
   );

   //fail to execute after invalidation
   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "exec"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("executer",      "alice")
                            ),
                            eosio_assert_message_exception,
                            eosio_assert_message_is("transaction authorization failed")
   );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( propose_invalidate_approve, eosio_msig_tester ) try {
   auto trx = reqauth( "alice"_n, {permission_level{"alice"_n, config::active_name}}, abi_serializer_max_time );

   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", vector<permission_level>{{ "alice"_n, config::active_name }})
   );

   //fail to execute before approval
   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "exec"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("executer",      "alice")
                            ),
                            eosio_assert_message_exception,
                            eosio_assert_message_is("transaction authorization failed")
   );

   //invalidate
   push_action( "alice"_n, "invalidate"_n, mvo()
                  ("account",      "alice")
   );

   //approve
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   //successfully execute

   auto r = push_action( "bob"_n, "exec"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("executer",      "bob")
   );

   BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
   BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( approve_execute_old, eosio_msig_tester ) try {
   set_code( "eosio.msig"_n, contracts::util::msig_wasm_old() );
   set_abi( "eosio.msig"_n, contracts::util::msig_abi_old().data() );
   produce_blocks();

   //propose with old version of eosio.msig
   auto trx = reqauth( "alice"_n, {permission_level{"alice"_n, config::active_name}}, abi_serializer_max_time );
   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", vector<permission_level>{{ "alice"_n, config::active_name }})
   );

   set_code( "eosio.msig"_n, contracts::msig_wasm() );
   set_abi( "eosio.msig"_n, contracts::msig_abi().data() );
   produce_blocks();

   //approve and execute with new version
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   auto r = push_action( "alice"_n, "exec"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("executer",      "alice")
   );

   BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
   BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( approve_unapprove_old, eosio_msig_tester ) try {
   set_code( "eosio.msig"_n, contracts::util::msig_wasm_old() );
   set_abi( "eosio.msig"_n, contracts::util::msig_abi_old().data() );
   produce_blocks();

   //propose with old version of eosio.msig
   auto trx = reqauth( "alice"_n, {permission_level{"alice"_n, config::active_name}}, abi_serializer_max_time );
   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", vector<permission_level>{{ "alice"_n, config::active_name }})
   );

   //approve with old version
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   set_code( "eosio.msig"_n, contracts::msig_wasm() );
   set_abi( "eosio.msig"_n, contracts::msig_abi().data() );
   produce_blocks();

   //unapprove with old version
   push_action( "alice"_n, "unapprove"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "exec"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("executer",      "alice")
                            ),
                            eosio_assert_message_exception,
                            eosio_assert_message_is("transaction authorization failed")
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( approve_by_two_old, eosio_msig_tester ) try {
   set_code( "eosio.msig"_n, contracts::util::msig_wasm_old() );
   set_abi( "eosio.msig"_n, contracts::util::msig_abi_old().data() );
   produce_blocks();

   auto trx = reqauth( "alice"_n, vector<permission_level>{ { "alice"_n, config::active_name }, { "bob"_n, config::active_name } }, abi_serializer_max_time );
   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", vector<permission_level>{ { "alice"_n, config::active_name }, { "bob"_n, config::active_name } })
   );

   //approve by alice
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
   );

   set_code( "eosio.msig"_n, contracts::msig_wasm() );
   set_abi( "eosio.msig"_n, contracts::msig_abi().data() );
   produce_blocks();

   //fail because approval by bob is missing
   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "exec"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("executer",      "alice")
                            ),
                            eosio_assert_message_exception,
                            eosio_assert_message_is("transaction authorization failed")
   );

   //approve and execute with new version
   push_action( "bob"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "bob"_n, config::active_name })
   );

   auto r = push_action( "alice"_n, "exec"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("executer",      "alice")
   );

   BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
   BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( approve_with_hash, eosio_msig_tester ) try {
   auto trx = reqauth( "alice"_n, {permission_level{"alice"_n, config::active_name}}, abi_serializer_max_time );
   auto trx_hash = fc::sha256::hash( trx );
   auto not_trx_hash = fc::sha256::hash( trx_hash );

   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx)
                  ("requested", vector<permission_level>{{ "alice"_n, config::active_name }})
   );

   //fail to approve with incorrect hash
   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "approve"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("level",         permission_level{ "alice"_n, config::active_name })
                                          ("proposal_hash", not_trx_hash)
                            ),
                            eosio::chain::crypto_api_exception,
                            fc_exception_message_is("hash mismatch")
   );

   //approve and execute
   push_action( "alice"_n, "approve"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("level",         permission_level{ "alice"_n, config::active_name })
                  ("proposal_hash", trx_hash)
   );

   auto r = push_action( "alice"_n, "exec"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("executer",      "alice")
   );

   BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
   BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( switch_proposal_and_fail_approve_with_hash, eosio_msig_tester ) try {
   auto trx1 = reqauth( "alice"_n, {permission_level{"alice"_n, config::active_name}}, abi_serializer_max_time );
   auto trx1_hash = fc::sha256::hash( trx1 );

   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx1)
                  ("requested", vector<permission_level>{{ "alice"_n, config::active_name }})
   );

   auto trx2 = reqauth( "alice"_n,
                       { permission_level{"alice"_n, config::active_name},
                         permission_level{"alice"_n, config::owner_name}  },
                       abi_serializer_max_time );

   push_action( "alice"_n, "cancel"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("canceler",       "alice")
   );

   push_action( "alice"_n, "propose"_n, mvo()
                  ("proposer",      "alice")
                  ("proposal_name", "first")
                  ("trx",           trx2)
                  ("requested", vector<permission_level>{ { "alice"_n, config::active_name },
                                                          { "alice"_n, config::owner_name } })
   );

   //fail to approve with hash meant for old proposal
   BOOST_REQUIRE_EXCEPTION( push_action( "alice"_n, "approve"_n, mvo()
                                          ("proposer",      "alice")
                                          ("proposal_name", "first")
                                          ("level",         permission_level{ "alice"_n, config::active_name })
                                          ("proposal_hash", trx1_hash)
                            ),
                            eosio::chain::crypto_api_exception,
                            fc_exception_message_is("hash mismatch")
   );
} FC_LOG_AND_RETHROW()

// WBP-2086: the node authorises the proposed transaction from its packed bytes and the contract then
// decodes the same bytes to send the actions. The two must read one transaction, so the contract accepts
// only the canonical encoding: decoding and re-encoding gives back the input, byte for byte.
// Each case runs against the build of this source and against the build deployed on mainnet.
BOOST_FIXTURE_TEST_CASE( propose_requires_canonical_trx, eosio_msig_tester ) try {
   const vector<permission_level> requested{{ "alice"_n, config::active_name }};
   transaction trx = reqauth( "alice"_n, requested, abi_serializer_max_time );
   const bytes canonical = fc::raw::pack( trx );
   // header: expiration (4), ref_block_num (2), ref_block_prefix (4), then max_net_usage_words as a varuint
   const size_t net_words_pos = 10;
   BOOST_REQUIRE_EQUAL( 0, canonical[net_words_pos] );
   BOOST_REQUIRE_EQUAL( 0, canonical.back() );   // zero transaction extensions

   auto with_last_prefix = [&]( std::initializer_list<uint8_t> prefix ) {
      bytes b( canonical.begin(), canonical.end() - 1 );
      b.insert( b.end(), prefix.begin(), prefix.end() );
      return b;
   };
   // bytes after the end of the transaction
   bytes trailing = canonical;
   trailing.push_back( 0 );
   // zero written in two bytes
   const bytes padded = with_last_prefix({ 0x80, 0x00 });
   // zero written in six bytes: more than a 32-bit varuint can take
   const bytes overlong = with_last_prefix({ 0x80, 0x80, 0x80, 0x80, 0x80, 0x00 });
   // five bytes with bits set above bit 31: the canonical form has the same length and a different last byte
   bytes high_bits( canonical.begin(), canonical.begin() + net_words_pos );
   for( uint8_t c : { 0xff, 0xff, 0xff, 0xff, 0x7f } ) high_bits.push_back( char(c) );
   high_bits.insert( high_bits.end(), canonical.begin() + net_words_pos + 1, canonical.end() );

   // a legitimate transaction whose header fields need multi-byte varuints
   trx.max_net_usage_words = 0xffffffff;
   trx.delay_sec           = 7 * 24 * 3600;
   const bytes wide = fc::raw::pack( trx );
   BOOST_REQUIRE( wide.size() > canonical.size() );

   auto run = [&]( const char* build ) {
      BOOST_TEST_CONTEXT( build ) {
         for( const auto& [proposal_name, packed] : std::vector<std::pair<name, bytes>>{
                 { "trailing"_n, trailing }, { "padded"_n, padded }, { "overlong"_n, overlong }, { "highbits"_n, high_bits } } ) {
            BOOST_TEST_CONTEXT( proposal_name.to_string() ) {
               BOOST_REQUIRE_EXCEPTION( propose_raw( "alice"_n, proposal_name, requested, packed ),
                                        eosio_assert_message_exception,
                                        eosio_assert_message_is("transaction is not canonically serialized") );
               BOOST_REQUIRE( get_row_by_account( "eosio.msig"_n, "alice"_n, "proposal"_n, proposal_name ).empty() );
            }
         }

         propose_raw( "alice"_n, "wide"_n, requested, wide );
         push_action( "alice"_n, "cancel"_n, mvo()("proposer", "alice")("proposal_name", "wide")("canceler", "alice") );

         // the canonical bytes are accepted and the proposal runs as before
         propose_raw( "alice"_n, "canonical"_n, requested, canonical );
         push_action( "alice"_n, "approve"_n, mvo()
                        ("proposer",      "alice")
                        ("proposal_name", "canonical")
                        ("level",         permission_level{ "alice"_n, config::active_name })
         );
         auto r = push_action( "alice"_n, "exec"_n, mvo()
                        ("proposer",      "alice")
                        ("proposal_name", "canonical")
                        ("executer",      "alice")
         );
         BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
         BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );
      }
   };

   run( "build of this source" );

   set_code( "eosio.msig"_n, contracts::util::msig_wasm_deployed_2026() );
   set_abi( "eosio.msig"_n, contracts::util::msig_abi_deployed().data() );
   produce_blocks();
   run( "build deployed on mainnet 2026-11-11" );
} FC_LOG_AND_RETHROW()

// WBP-2086: `exec` applies the same rule, which covers a non-canonical proposal stored before the upgrade
// (here by the build that was live on mainnet until 2026-11-11). It can still collect approvals, cannot be
// executed, and its proposer can cancel it. A canonical proposal stored before the upgrade runs as before.
BOOST_FIXTURE_TEST_CASE( exec_requires_canonical_trx, eosio_msig_tester ) try {
   const vector<permission_level> requested{{ "alice"_n, config::active_name }};
   const bytes canonical = fc::raw::pack( reqauth( "alice"_n, requested, abi_serializer_max_time ) );
   bytes trailing = canonical;
   trailing.push_back( 0 );
   bytes overlong( canonical.begin(), canonical.end() - 1 );
   for( uint8_t c : { 0x80, 0x80, 0x80, 0x80, 0x80, 0x00 } ) overlong.push_back( char(c) );

   auto approve_and_exec = [&]( name proposal_name ) {
      push_action( "alice"_n, "approve"_n, mvo()
                     ("proposer",      "alice")
                     ("proposal_name", proposal_name)
                     ("level",         permission_level{ "alice"_n, config::active_name })
      );
      return push_action( "alice"_n, "exec"_n, mvo()
                     ("proposer",      "alice")
                     ("proposal_name", proposal_name)
                     ("executer",      "alice")
      );
   };

   auto run = [&]( const char* build, const std::vector<uint8_t>& wasm, const std::vector<char>& abi ) {
      BOOST_TEST_CONTEXT( build ) {
         set_code( "eosio.msig"_n, contracts::util::msig_wasm_deployed_2022() );
         set_abi( "eosio.msig"_n, contracts::util::msig_abi_deployed().data() );
         produce_blocks();
         propose_raw( "alice"_n, "trailing"_n,  requested, trailing );
         propose_raw( "alice"_n, "overlong"_n,  requested, overlong );
         propose_raw( "alice"_n, "canonical"_n, requested, canonical );

         set_code( "eosio.msig"_n, wasm );
         set_abi( "eosio.msig"_n, abi.data() );
         produce_blocks();

         for( name proposal_name : { "trailing"_n, "overlong"_n } ) {
            BOOST_TEST_CONTEXT( proposal_name.to_string() ) {
               BOOST_REQUIRE_EXCEPTION( approve_and_exec( proposal_name ),
                                        eosio_assert_message_exception,
                                        eosio_assert_message_is("transaction is not canonically serialized") );
               BOOST_REQUIRE( !get_row_by_account( "eosio.msig"_n, "alice"_n, "proposal"_n, proposal_name ).empty() );
               push_action( "alice"_n, "cancel"_n, mvo()("proposer", "alice")("proposal_name", proposal_name)("canceler", "alice") );
               BOOST_REQUIRE( get_row_by_account( "eosio.msig"_n, "alice"_n, "proposal"_n, proposal_name ).empty() );
               BOOST_REQUIRE( get_row_by_account( "eosio.msig"_n, "alice"_n, "approvals2"_n, proposal_name ).empty() );
            }
         }

         auto r = approve_and_exec( "canonical"_n );
         BOOST_REQUIRE_EQUAL( 2, r->action_traces.size() );
         BOOST_REQUIRE_EQUAL( transaction_receipt::executed, r->receipt->status );
      }
   };

   run( "build of this source", contracts::msig_wasm(), contracts::msig_abi() );
   run( "build deployed on mainnet 2026-11-11", contracts::util::msig_wasm_deployed_2026(), contracts::util::msig_abi_deployed() );
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
