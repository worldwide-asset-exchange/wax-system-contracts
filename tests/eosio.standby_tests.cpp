#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <fc/log/logger.hpp>
#include <iostream>
#include <sstream>
#include <random>
#include <set>

#include "eosio.system_tester.hpp"

inline const auto alice = "alice1111111"_n;
inline const auto bob = "bob111111111"_n;


// bool within_error(int64_t a, int64_t b, int64_t err) { return std::abs(a - b) <= err; };
// bool within_one(int64_t a, int64_t b) { return within_error(a, b, 1); }
// bool within_and_gte(int64_t a, int64_t b, int64_t w) { return a - b <= w && a >= b; }

struct standby_producer_state {
  name            owner;
  u_int64_t       standby_share = 0;
  time_point      last_standby_share_update;
  time_point      last_claim_time;
  bool            is_active = true;
};

FC_REFLECT(standby_producer_state, (owner)(standby_share)(last_standby_share_update)(last_claim_time)(is_active))

struct standby_disallow_state {
  name            owner;
};

FC_REFLECT(standby_disallow_state, (owner))


using namespace eosio_system;


bool within_error(int64_t a, int64_t b, int64_t err) { return std::abs(a - b) <= err; };
bool within_one(int64_t a, int64_t b) { return within_error(a, b, 1); }
bool within_and_gte(int64_t a, int64_t b, int64_t w) { return a - b <= w && a >= b; }



struct eosio_standby_tester : eosio_system_tester {
  eosio_standby_tester() {  }

  standby_producer_state get_standby_producer_state(name acc) {
   vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "standbys"_n, acc);
    return fc::raw::unpack<standby_producer_state>(data);
  }

  standby_disallow_state get_standby_disallow_state(name acc) {
    vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "sbdisallow"_n, acc);
    return fc::raw::unpack<standby_disallow_state>(data);
  }

  fc::variant get_global_state4() {
    vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, "global4"_n, "global4"_n );
    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "eosio_global_state4", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
  }

  std::vector<standby_producer_state> get_standby_table()
  {
    std::vector<standby_producer_state> result;

    const auto* table_id_itr = control->db().find<eosio::chain::table_id_object, eosio::chain::by_code_scope_table>(
      boost::make_tuple(eosio::chain::config::system_account_name, eosio::chain::config::system_account_name, "standbys"_n));

    const auto& idx = control->db().get_index<eosio::chain::key_value_index, eosio::chain::by_scope_primary>();
    auto table_id = table_id_itr->id;

    standby_producer_state r;

    auto lower = idx.lower_bound(boost::make_tuple(table_id, 0));
    for (auto itr = lower; itr != idx.end() && itr->t_id == table_id; ++itr){
         fc::datastream<const char*> ds(itr->value.data(), itr->value.size());
         fc::raw::unpack(ds, r);
         result.push_back(r);
    }
    return result;
  }

  vector<name> active_and_vote_producers_and_standbys() {
    //stake more than 15% of total EOS supply to activate chain
    const asset net = core_sym::from_string("80.0000");
    const asset cpu = core_sym::from_string("80.0000");
    const std::vector<account_name> voters = { "producvotera"_n, "producvoterb"_n, "producvoterc"_n, "producvoterd"_n };
    for (const auto& v: voters) {
       create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
       transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
       BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
    }
 
    // create accounts {defproducera, defproducerb, ..., defproducerz, abcproducera, ..., defproducern} and register as producers
    std::vector<account_name> producer_names;
    {
       producer_names.reserve('z' - 'a' + 1);
       {
          const std::string root("defproducer");
          for ( char c = 'a'; c <= 'z'; ++c ) {
             producer_names.emplace_back(root + std::string(1, c));
          }
       }
       {
          const std::string root("zzzproducer");
          for ( char c = 'a'; c <= 'n'; ++c ) {
             producer_names.emplace_back(root + std::string(1, c));
          }
       }
       setup_producer_accounts(producer_names);
       for (const auto& p: producer_names) {
          BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
          produce_blocks(1);
          ilog( "------ get pro----------" );
          wdump((p));
          BOOST_TEST(0 == get_producer_info(p)["total_votes"].as<double>());
       }
    }
 
    produce_block( fc::hours(24) );
 
    {
       BOOST_REQUIRE_EQUAL(success(), vote("producvotera"_n, vector<account_name>(producer_names.begin(), producer_names.begin()+10)));
       BOOST_REQUIRE_EQUAL(success(), vote("producvoterb"_n, vector<account_name>(producer_names.begin(), producer_names.begin()+21)));
       BOOST_REQUIRE_EQUAL(success(), vote("producvoterc"_n, vector<account_name>(producer_names.begin(), producer_names.begin()+26)));
       BOOST_REQUIRE_EQUAL(success(), vote("producvoterd"_n, vector<account_name>(producer_names.begin()+26, producer_names.end())));
    }
    produce_blocks(23 * 12 + 20);

    auto producer_keys = control->head_block_state()->active_schedule.producers;
    BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );
    BOOST_REQUIRE_EQUAL( name("defproducera"), producer_keys[0].producer_name );

    return producer_names;
  }

};


BOOST_AUTO_TEST_SUITE(eosio_standby_tests)

BOOST_FIXTURE_TEST_CASE(standby_config_tests, eosio_standby_tester ) try {
  const auto& gs4 = get_global_state4();
  BOOST_TEST_REQUIRE( 0 == gs4["standby_slot_weight"].as_uint64() );
  BOOST_TEST_REQUIRE( 0 == gs4["num_standby_slots"].as_uint64() );
  BOOST_TEST_REQUIRE( 0 == gs4["total_standby_share"].as_uint64() );


  // Test invalid values for setsbratio
  BOOST_REQUIRE_EQUAL( 
    wasm_assert_msg("ratio cannot exceed PAY_SPLIT_SCALE (10000)"),
    push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", -1) )
  );

  BOOST_REQUIRE_EQUAL( 
    wasm_assert_msg("ratio cannot exceed PAY_SPLIT_SCALE (10000)"),
    push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", 10001) )
  );


  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", 5000) )
  );

  const auto& gs42 = get_global_state4();
  BOOST_TEST_REQUIRE( 5000 == gs42["standby_slot_weight"].as_uint64() );


  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbslot"_n, mvo()("num_slots", 5) )
  );

  const auto& gs43 = get_global_state4();
  BOOST_TEST_REQUIRE( 5 == gs43["num_standby_slots"].as_uint64() );

} 
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(standby_disallow_tests, eosio_standby_tester ) try {
  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "disallowsb"_n, mvo()("account", alice) )
  );

  const auto& sps = get_standby_disallow_state(alice);
  BOOST_TEST_REQUIRE( alice == sps.owner );
} 
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(standby_list, eosio_standby_tester ) try {
  
  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", 5000) )
  );

  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbslot"_n, mvo()("num_slots", 5) )
  );

  auto producer_names = active_and_vote_producers_and_standbys();
  // fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

  ilog( "------ get producers----------" );
  wdump((producer_names));

  auto producer_keys = control->head_block_state()->active_schedule.producers;

  wdump((producer_keys));

  const auto& gs4 = get_global_state4();
  BOOST_TEST_REQUIRE( 5000 == gs4["standby_slot_weight"].as_uint64() );
  BOOST_TEST_REQUIRE( 5 == gs4["num_standby_slots"].as_uint64() );
  BOOST_TEST_REQUIRE( 0 == gs4["total_standby_share"].as_uint64() );

  auto standby_producers = get_standby_table();
  wdump((standby_producers));
  BOOST_TEST_REQUIRE( 5 == standby_producers.size() );
  BOOST_TEST_REQUIRE( name("defproducerv") == standby_producers[0].owner );
  BOOST_TEST_REQUIRE( name("defproducerw") == standby_producers[1].owner );
  BOOST_TEST_REQUIRE( name("defproducerx") == standby_producers[2].owner );
  BOOST_TEST_REQUIRE( name("defproducery") == standby_producers[3].owner );
  BOOST_TEST_REQUIRE( name("defproducerz") == standby_producers[4].owner );
  // fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);

}
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(standby_claims, eosio_standby_tester ) try {
  
  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", 5000) )
  );

  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbslot"_n, mvo()("num_slots", 5) )
  );

  auto producer_names = active_and_vote_producers_and_standbys();
  ilog( "------ get producers----------" );
  wdump((producer_names));

  auto producer_keys = control->head_block_state()->active_schedule.producers;

  wdump((producer_keys));

  const auto& gs4 = get_global_state4();
  BOOST_TEST_REQUIRE( 5000 == gs4["standby_slot_weight"].as_uint64() );
  BOOST_TEST_REQUIRE( 5 == gs4["num_standby_slots"].as_uint64() );
  BOOST_TEST_REQUIRE( 0 == gs4["total_standby_share"].as_uint64() );

  auto standby_producers = get_standby_table();
  wdump((standby_producers));

  {
    produce_blocks(23 * 12 + 20);
    bool all_21_produced = true;
    for (uint32_t i = 0; i < 21; ++i) {
       if (0 == get_producer_info(producer_names[i])["unpaid_blocks"].as<uint32_t>()) {
          all_21_produced = false;
       }
    }
    bool rest_didnt_produce = true;
    for (uint32_t i = 21; i < producer_names.size(); ++i) {
       if (0 < get_producer_info(producer_names[i])["unpaid_blocks"].as<uint32_t>()) {
          rest_didnt_produce = false;
       }
    }
    BOOST_REQUIRE(all_21_produced && rest_didnt_produce);
 }

  produce_blocks(5);

  
  const u_int32_t standby_index = 2;
  const auto& stbd_producer = standby_producers[standby_index];  
  const auto& prod_name = stbd_producer.owner;

  const asset initial_balance  = get_balance(prod_name);

  BOOST_REQUIRE_EQUAL(success(), push_action(prod_name, "claimstandby"_n, mvo()("owner", prod_name)));
  
  const asset    balance           = get_balance(prod_name);

  ilog("before balance:                  ${x}", ("x", initial_balance.get_amount()));
  ilog("after balance:                   ${x}", ("x", balance.get_amount()));

  BOOST_TEST_REQUIRE( initial_balance.get_amount() < balance.get_amount());

  
  auto standby_producers2 = get_standby_table();
  wdump((standby_producers2));

  const auto& gs41 = get_global_state4();
  const double standby_bucket = gs41["standby_bucket"].as_uint64();
  const double total_standby_share = gs41["total_standby_share"].as_uint64();

  ilog("total_standby_share: ${x}", ("x", gs41["total_standby_share"].as_uint64()));
  ilog("standby bucket: ${x}", ("x", gs41["standby_bucket"].as_uint64()));

  BOOST_REQUIRE( within_one(standby_bucket / 4, balance.get_amount() - initial_balance.get_amount() ) );

}
FC_LOG_AND_RETHROW()



BOOST_FIXTURE_TEST_CASE(change_standbys_active, eosio_standby_tester ) try {

  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", 5000) )
  );

  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbslot"_n, mvo()("num_slots", 5) )
  );
  auto producer_names = active_and_vote_producers_and_standbys();


  // change vote to make standbys change
  // producers: b-w
  // retain standbys: defproducery, defproducerz
  // add new standbys 
  {
    BOOST_REQUIRE_EQUAL(success(), vote("producvotera"_n, vector<account_name>(producer_names.begin()+1, producer_names.begin()+22)));
    BOOST_REQUIRE_EQUAL(success(), vote("producvoterb"_n, vector<account_name>(producer_names.begin()+1, producer_names.begin()+22)));
    BOOST_REQUIRE_EQUAL(success(), vote("producvoterc"_n, vector<account_name>(producer_names.begin()+1, producer_names.begin()+22)));
    BOOST_REQUIRE_EQUAL(success(), vote("producvoterd"_n, vector<account_name>(producer_names.begin()+24, producer_names.end())));
  }
  produce_block( fc::hours(24) );

  
  auto standby_producers2 = get_standby_table();
  wdump((standby_producers2));


  BOOST_TEST_REQUIRE( 8 == standby_producers2.size() );
  BOOST_TEST_REQUIRE( name("defproducerv") == standby_producers2[0].owner );
  BOOST_TEST_REQUIRE( name("defproducerw") == standby_producers2[1].owner );
  BOOST_TEST_REQUIRE( name("defproducerx") == standby_producers2[2].owner );
  BOOST_TEST_REQUIRE( name("defproducery") == standby_producers2[3].owner );
  BOOST_TEST_REQUIRE( name("defproducerz") == standby_producers2[4].owner );
  BOOST_TEST_REQUIRE( name("zzzproducera") == standby_producers2[5].owner );
  BOOST_TEST_REQUIRE( name("zzzproducerb") == standby_producers2[6].owner );
  BOOST_TEST_REQUIRE( name("zzzproducerc") == standby_producers2[7].owner );

  BOOST_TEST_REQUIRE( standby_producers2[0].is_active == false );
  BOOST_TEST_REQUIRE( standby_producers2[1].is_active == false );
  BOOST_TEST_REQUIRE( standby_producers2[2].is_active == false );
  BOOST_TEST_REQUIRE( standby_producers2[3].is_active == true );
  BOOST_TEST_REQUIRE( standby_producers2[4].is_active == true );
  BOOST_TEST_REQUIRE( standby_producers2[5].is_active == true );
  BOOST_TEST_REQUIRE( standby_producers2[6].is_active == true );
  BOOST_TEST_REQUIRE( standby_producers2[7].is_active == true );

}
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(standby_producer_pay, eosio_standby_tester,  * boost::unit_test::tolerance(1e-10)) try {
  auto within_one = [](int64_t a, int64_t b) -> bool { return std::abs( a - b ) <= 1; };

  const int64_t secs_per_year  = 52 * 7 * 24 * 3600;
  const double  usecs_per_year = secs_per_year * 1000000;
  const double  cont_rate      = 0.04879;;

  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);
  const u_int64_t SB_RATIO = 5000;
  const u_int32_t SB_SLOTS = 5;

  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", SB_RATIO) )
  );

  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbslot"_n, mvo()("num_slots", SB_SLOTS) )
  );
  auto producer_names = active_and_vote_producers_and_standbys();
  wdump((producer_names));

  auto producer_keys = control->head_block_state()->active_schedule.producers;

  wdump((producer_keys));

  {
    bool all_21_produced = true;
    for (uint32_t i = 0; i < 21; ++i) {
      if (0 == get_producer_info(producer_names[i])["unpaid_blocks"].as<uint32_t>()) {
          all_21_produced = false;
      }
    }
    bool rest_didnt_produce = true;
    for (uint32_t i = 21; i < producer_names.size(); ++i) {
      if (0 < get_producer_info(producer_names[i])["unpaid_blocks"].as<uint32_t>()) {
          rest_didnt_produce = false;
      }
    }
    BOOST_REQUIRE(all_21_produced && rest_didnt_produce);
  }


  std::vector<double> vote_shares(producer_names.size());
  {
     double total_votes = 0;
     for (uint32_t i = 0; i < producer_names.size(); ++i) {
        vote_shares[i] = get_producer_info(producer_names[i])["total_votes"].as<double>();
        total_votes += vote_shares[i];
     }
     BOOST_TEST(total_votes == get_global_state()["total_producer_vote_weight"].as<double>());
     std::for_each(vote_shares.begin(), vote_shares.end(), [total_votes](double& x) { x /= total_votes; });

     BOOST_TEST(double(1) == std::accumulate(vote_shares.begin(), vote_shares.end(), double(0)));
     BOOST_TEST(double(3./71.) == vote_shares.front());
     BOOST_TEST(double(1./71.) == vote_shares.back());
  }

  {
    const uint32_t prod_index = 2;
    const auto prod_name = producer_names[prod_index];

    const auto     initial_global_state      = get_global_state();
    const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );
    const int64_t  initial_pervote_bucket    = initial_global_state["pervote_bucket"].as<int64_t>();
    const int64_t  initial_perblock_bucket   = initial_global_state["perblock_bucket"].as<int64_t>();
    const int64_t  initial_savings           = get_balance("eosio.saving"_n).get_amount();
    const uint32_t initial_tot_unpaid_blocks = initial_global_state["total_unpaid_blocks"].as<uint32_t>();
    const asset    initial_supply            = get_token_supply();
    const asset    initial_bpay_balance      = get_balance("eosio.bpay"_n);
    const asset    initial_balance           = get_balance(prod_name);
    const uint32_t initial_unpaid_blocks     = get_producer_info(prod_name)["unpaid_blocks"].as<uint32_t>();

    const auto initial_global4_state = get_global_state4();
    const int64_t initial_standby_bucket = initial_global4_state["standby_bucket"].as<int64_t>();

    ilog("initial_claim_time: ${x}", ("x", initial_claim_time));
    ilog("initial_pervote_bucket: ${x}", ("x", initial_pervote_bucket));
    ilog("initial_perblock_bucket: ${x}", ("x", initial_perblock_bucket));
    ilog("initial_savings: ${x}", ("x", initial_savings));
    ilog("initial_tot_unpaid_blocks: ${x}", ("x", initial_tot_unpaid_blocks));
    ilog("initial_supply: ${x}", ("x", initial_supply));
    ilog("initial_bpay_balance: ${x}", ("x", initial_bpay_balance));
    ilog("initial_balance: ${x}", ("x", initial_balance));
    ilog("initial_unpaid_blocks: ${x}", ("x", initial_unpaid_blocks));
    ilog("initial_standby_bucket: ${x}", ("x", initial_standby_bucket));

    BOOST_REQUIRE_EQUAL(success(), push_action(prod_name, "claimrewards"_n, mvo()("owner", prod_name)));

    const auto     global_state      = get_global_state();
    const uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );
    const int64_t  pervote_bucket    = global_state["pervote_bucket"].as<int64_t>();
    const int64_t  perblock_bucket   = global_state["perblock_bucket"].as<int64_t>();
    const int64_t  savings           = get_balance("eosio.saving"_n).get_amount();
    const uint32_t tot_unpaid_blocks = global_state["total_unpaid_blocks"].as<uint32_t>();
    const asset    supply            = get_token_supply();
    const asset    bpay_balance      = get_balance("eosio.bpay"_n);
    const asset    balance           = get_balance(prod_name);
    const uint32_t unpaid_blocks     = get_producer_info(prod_name)["unpaid_blocks"].as<uint32_t>();

    const auto global4_state_1 = get_global_state4();
    const int64_t standby_bucket = global4_state_1["standby_bucket"].as<int64_t>();

    ilog("claim_time: ${x}", ("x", claim_time));
    ilog("pervote_bucket: ${x}", ("x", pervote_bucket));
    ilog("perblock_bucket: ${x}", ("x", perblock_bucket));
    ilog("savings: ${x}", ("x", savings));
    ilog("tot_unpaid_blocks: ${x}", ("x", tot_unpaid_blocks));
    ilog("supply: ${x}", ("x", supply));
    ilog("bpay_balance: ${x}", ("x", bpay_balance));
    ilog("balance: ${x}", ("x", balance));
    ilog("unpaid_blocks: ${x}", ("x", unpaid_blocks));
    ilog("standby_bucket: ${x}", ("x", standby_bucket));

    const uint64_t usecs_between_fills = claim_time - initial_claim_time;
    const int32_t secs_between_fills = static_cast<int32_t>(usecs_between_fills / 1000000);

    const double expected_supply_growth = initial_supply.get_amount() * double(usecs_between_fills) * cont_rate / usecs_per_year;
    BOOST_REQUIRE_EQUAL( int64_t(expected_supply_growth), supply.get_amount() - initial_supply.get_amount() );

    // saving 3/10
    BOOST_REQUIRE_EQUAL( int64_t(expected_supply_growth) - (int64_t(expected_supply_growth) * 3 / 10) - (int64_t(expected_supply_growth) * 4 / 10), savings - initial_savings );

    // perblock 3/10
    const int64_t original_perblock_bucket = int64_t( double(initial_supply.get_amount()) * double(usecs_between_fills) * (cont_rate * 3. / 10.) / usecs_per_year );
    const int64_t expected_pervote_bucket  = 0;

    // producer pay now reduced for standby
    const int64_t expected_producer_pay_bucket = original_perblock_bucket * 21 * 10000/ (21 * 10000 + SB_RATIO * SB_SLOTS);
    const int64_t standby_pay_bucket = original_perblock_bucket - expected_producer_pay_bucket;

    const int64_t from_perblock_bucket = initial_unpaid_blocks * expected_producer_pay_bucket / initial_tot_unpaid_blocks ;
    const int64_t from_pervote_bucket  = 0;

    BOOST_REQUIRE( 1 >= abs(int32_t(initial_tot_unpaid_blocks - tot_unpaid_blocks) - int32_t(initial_unpaid_blocks - unpaid_blocks)) );


    ilog("Expected Per-block bucket: ${x}", ("x", original_perblock_bucket));
    ilog("Expected expected_producer_pay_bucket: ${x}", ("x", expected_producer_pay_bucket));
    ilog("From perblock bucket: ${x}", ("x", from_perblock_bucket));
    ilog("Standby pay bucket: ${x}", ("x", standby_pay_bucket));

    BOOST_REQUIRE( within_one( from_perblock_bucket, balance.get_amount() - initial_balance.get_amount() ) );
    BOOST_REQUIRE( within_one( expected_pervote_bucket, pervote_bucket ) );
    BOOST_REQUIRE( within_one( perblock_bucket, bpay_balance.get_amount() - standby_pay_bucket));
    
    // verify the standby bucket
    BOOST_REQUIRE( within_one( standby_pay_bucket, standby_bucket - initial_standby_bucket));
    
    // produce_blocks(5);

    // BOOST_REQUIRE_EQUAL(wasm_assert_msg("already claimed rewards within past day"),
                        // push_action(prod_name, "claimrewards"_n, mvo()("owner", prod_name)));
  }

  {
    auto standby_producers = get_standby_table();
    wdump((standby_producers));
    auto standby_name = standby_producers[0].owner; // defproducerv
    
    auto initial_global4_state = get_global_state4();
    const int64_t initial_standby_bucket = initial_global4_state["standby_bucket"].as<int64_t>();
    const int64_t initial_total_standby_share = initial_global4_state["total_standby_share"].as<int64_t>();
    const asset initial_balance  = get_balance(standby_name);

    ilog("initial_standby_bucket: ${x}", ("x", initial_standby_bucket));
    ilog("initial_total_standby_share: ${x}", ("x", initial_total_standby_share));
    ilog("before balance:                  ${x}", ("x", initial_balance.get_amount()));

    BOOST_REQUIRE_EQUAL(success(), push_action(standby_name, "claimstandby"_n, mvo()("owner", standby_name)));
  
    const auto     global4_state_1     = get_global_state4();
    const int64_t  standby_bucket_1    = global4_state_1["standby_bucket"].as<int64_t>();
    const int64_t  total_standby_share_1 = global4_state_1["total_standby_share"].as<int64_t>();
    const asset    balance           = get_balance(standby_name);

    ilog("standby_bucket:                   ${x}", ("x", standby_bucket_1));
    ilog("total_standby_share:                   ${x}", ("x", total_standby_share_1));
    ilog("after balance:                   ${x}", ("x", balance.get_amount()));

    auto standby_producers_2 = get_standby_table();
    wdump((standby_producers_2));
    const int64_t total_share_left = standby_producers_2[0].standby_share;
    const int64_t share_of_each_standby = standby_producers_2[1].standby_share; // should be equal

    BOOST_REQUIRE_EQUAL(0, total_share_left);
    BOOST_REQUIRE_EQUAL(share_of_each_standby, total_standby_share_1 / 4);

    BOOST_REQUIRE( within_one(standby_bucket_1 / 4, balance.get_amount() - initial_balance.get_amount() ) );

    /// next standby producer claim

    const auto     initial_global_state      = get_global_state();
    const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );
    const int64_t  initial_savings           = get_balance("eosio.saving"_n).get_amount();
    const asset    initial_supply            = get_token_supply();

    auto standby_name2 = standby_producers_2[1].owner; // defproducerw
    const asset initial_balance2  = get_balance(standby_name2);
    ilog("initial_balance2:                   ${x}", ("x", initial_balance2.get_amount()));

    // CLAIM ACTION
    BOOST_REQUIRE_EQUAL(success(), push_action(standby_name2, "claimstandby"_n, mvo()("owner", standby_name2)));


    const auto     global4_state_2     = get_global_state4();
    const int64_t  standby_bucket_2    = global4_state_2["standby_bucket"].as<int64_t>();
    const int64_t  total_standby_share_2 = global4_state_2["total_standby_share"].as<int64_t>();
    const asset balance2 = get_balance(standby_name2);

    const auto     global_state      = get_global_state();
    const uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );
    const int64_t  savings           = get_balance("eosio.saving"_n).get_amount();
    const asset    supply            = get_token_supply();

    auto standby_producers_3 = get_standby_table();
    wdump((standby_producers_3));

    ilog("standby_bucket2:                   ${x}", ("x", standby_bucket_2));
    ilog("total_standby_share2:                   ${x}", ("x", total_standby_share_2));
    ilog("after balance2:                   ${x}", ("x", balance2.get_amount()));

    int64_t share_increase = standby_producers_3[0].standby_share;
    // standby 2 claimed, 3 should have increased share
    int64_t share_of_each_standby_3 = standby_producers_3[3].standby_share; // should be equal

    // share increase in the time should equal new total share + share of the standby just claim
    BOOST_REQUIRE_EQUAL(total_standby_share_1 + share_increase * 5, total_standby_share_2 + share_of_each_standby_3);

    // calculate the amount just claimed
    // int64_t expected_claim_3 = standby_bucket_2 * share_of_each_standby_3 / (total_standby_share_2 + share_of_each_standby_3);

    const uint64_t usecs_between_fills = claim_time - initial_claim_time;
    const int32_t secs_between_fills = static_cast<int32_t>(usecs_between_fills / 1000000);

    const double expected_supply_growth = initial_supply.get_amount() * double(usecs_between_fills) * cont_rate / usecs_per_year;
    BOOST_REQUIRE_EQUAL( int64_t(expected_supply_growth), supply.get_amount() - initial_supply.get_amount() );

    // saving 3/10
    BOOST_REQUIRE_EQUAL( int64_t(expected_supply_growth) - (int64_t(expected_supply_growth) * 3 / 10) - (int64_t(expected_supply_growth) * 4 / 10), savings - initial_savings );

    // perblock 3/10
    const int64_t original_perblock_bucket = int64_t( double(initial_supply.get_amount()) * double(usecs_between_fills) * (cont_rate * 3. / 10.) / usecs_per_year );
    const int64_t expected_pervote_bucket  = 0;

    // producer pay now reduced for standby
    const int64_t expected_producer_pay_bucket = original_perblock_bucket * 21 * 10000/ (21 * 10000 + SB_RATIO * SB_SLOTS);
    const int64_t standby_pay_bucket = original_perblock_bucket - expected_producer_pay_bucket;

    ilog("Expected Per-block bucket: ${x}", ("x", original_perblock_bucket));
    ilog("Expected expected_producer_pay_bucket: ${x}", ("x", expected_producer_pay_bucket));
    ilog("Standby pay bucket: ${x}", ("x", standby_pay_bucket));

    const int64_t expected_claimed = (standby_bucket_1 + standby_pay_bucket) * share_of_each_standby_3 /(total_standby_share_2 + share_of_each_standby_3);
    ilog("expected_claimed: ${x}", ("x", expected_claimed));
    BOOST_REQUIRE( within_one( expected_claimed, balance2.get_amount() - initial_balance2.get_amount()));

  }

  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);

}
FC_LOG_AND_RETHROW()

// ---------------------------------------------------------------------------
// WCAP-SYS-2026-002 (WBP-1990). Three privileged setters compared an unsigned parameter
// with `>= 0`, which is always true, so the lower bound they promised was never enforced
// and one message named a constant the code did not compare against. Each now enforces
// only a real bound and says which one. setsbslot also gains the ceiling it never had.
// ---------------------------------------------------------------------------
BOOST_FIXTURE_TEST_CASE( wcap_002_unsigned_setters_enforce_real_bounds, eosio_standby_tester ) try {
   const name eosio = config::system_account_name;
   auto setsbratio = [&]( int64_t ratio )   { return push_action( eosio, "setsbratio"_n, mvo()("ratio", ratio) ); };
   auto setsbslot  = [&]( int64_t slots )   { return push_action( eosio, "setsbslot"_n,  mvo()("num_slots", slots) ); };
   auto setrngrate = [&]( int64_t rate )    { return push_action( eosio, "setrngrate"_n, mvo()("rng_rate", rate)("max_pool_rng", 1000000000) ); };

   // setsbratio: the only meaningful bound is the upper one, and the message names it.
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("ratio cannot exceed PAY_SPLIT_SCALE (10000)"), setsbratio( 10001 ) );
   BOOST_REQUIRE_EQUAL( success(), setsbratio( 10000 ) );
   BOOST_REQUIRE_EQUAL( success(), setsbratio( 0 ) );

   // setsbslot: zero is valid (disables standbys); the list is capped at the active-schedule size.
   BOOST_REQUIRE_EQUAL( success(), setsbslot( 0 ) );
   BOOST_REQUIRE_EQUAL( 0u, get_global_state4()["num_standby_slots"].as_uint64() );
   BOOST_REQUIRE_EQUAL( success(), setsbslot( 21 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("num_slots cannot exceed 21"), setsbslot( 22 ) );
   BOOST_REQUIRE_EQUAL( 21u, get_global_state4()["num_standby_slots"].as_uint64() );

   // setrngrate: the upper bound is exclusive and the message says so.
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("rng_rate must be less than 10000"), setrngrate( 10000 ) );
   BOOST_REQUIRE_EQUAL( success(), setrngrate( 9999 ) );
   BOOST_REQUIRE_EQUAL( success(), setrngrate( 0 ) );
} FC_LOG_AND_RETHROW()

// ---------------------------------------------------------------------------
// WCAP-SYS-2026-006 (WBP-1994). claimstandby computed its payout in doubles with no upper
// clamp, then subtracted the double from a uint64 bucket; collect_voter_reward, the same
// computation one file over, clamps. Filed as a missing safety net, not a demonstrated
// exploit. Two tests: the one observable pre/post difference (a reward that rounds to zero
// is refused before the bucket is touched, instead of failing inside the token transfer),
// and the property both claim paths must hold.
// ---------------------------------------------------------------------------
BOOST_FIXTURE_TEST_CASE( wcap_006_fractional_standby_reward_is_refused_cleanly, eosio_standby_tester ) try {
   // Four standbys accrue share for a day; a fifth slot is then opened and its new standby
   // claims within a block or two of promotion. Its share is seconds against days, so its
   // cut of the bucket is a fraction of a unit. Before the fix `amount > 0` passed, the
   // bucket was decremented by the fraction, and the action died inside eosio.token with
   // "must transfer positive quantity"; now the guard refuses before the bucket is touched.
   const name eosio = config::system_account_name;
   BOOST_REQUIRE_EQUAL( success(), push_action( eosio, "setsbratio"_n, mvo()("ratio", 1) ) );
   BOOST_REQUIRE_EQUAL( success(), push_action( eosio, "setsbslot"_n,  mvo()("num_slots", 4) ) );
   active_and_vote_producers_and_standbys();
   std::set<name> first_four;
   for( const auto& s : get_standby_table() ) if( s.is_active ) first_four.insert( s.owner );
   BOOST_REQUIRE_EQUAL( 4u, first_four.size() );
   produce_block( fc::days(1) );

   BOOST_REQUIRE_EQUAL( success(), push_action( eosio, "setsbslot"_n, mvo()("num_slots", 5) ) );
   name fifth;
   for( int i = 0; i < 400 && fifth == name(); ++i ) {
      produce_blocks(1);
      for( const auto& s : get_standby_table() )
         if( s.is_active && !first_four.count( s.owner ) ) fifth = s.owner;
   }
   BOOST_REQUIRE( fifth != name() );
   produce_blocks(4);   // two seconds of share against four standby-days

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("no standby reward to claim"),
                        push_action( fifth, "claimstandby"_n, mvo()("owner", fifth) ) );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_006_bucket_solvency_property, eosio_standby_tester ) try {
   // Property, over a fixed-seed random sequence of claims by standbys and voters with random
   // gaps: a claim never pays more than its bucket held plus what the claim itself credited,
   // the bucket after the claim accounts for exactly what was paid, and neither bucket nor
   // share total ever wraps. Buckets are filled only inside claims, for the time since
   // global.last_pervote_bucket_fill, so that timestamp bounds each claim's credit from the
   // inflation formula. Held to both claimstandby and collect_voter_reward, because they are
   // the same computation.
   const name eosio = config::system_account_name;
   BOOST_REQUIRE_EQUAL( success(), push_action( eosio, "setsbratio"_n, mvo()("ratio", 5000) ) );
   BOOST_REQUIRE_EQUAL( success(), push_action( eosio, "setsbslot"_n,  mvo()("num_slots", 5) ) );
   active_and_vote_producers_and_standbys();
   std::vector<name> standby_names;
   for( const auto& s : get_standby_table() ) if( s.is_active ) standby_names.push_back( s.owner );
   BOOST_REQUIRE_EQUAL( 5u, standby_names.size() );
   const std::vector<name> voters = { "producvotera"_n, "producvoterb"_n, "producvoterc"_n, "producvoterd"_n };

   // Upper bound on what one claim can credit to a bucket: that bucket's share of the
   // continuous 5%/yr inflation (the contract's 52-week year) over the gap since the last
   // fill, with 5% slack for the claim's own block. Block pay is 3/10 of inflation and the
   // standby bucket gets weight*slots / (apc*PAY_SPLIT_SCALE + weight*slots) of block pay;
   // voters get 4/10 of inflation.
   const auto gs4 = get_global_state4();
   const double standby_fraction = 0.3 * double( gs4["standby_slot_weight"].as_uint64() * gs4["num_standby_slots"].as_uint64() )
      / double( gs4["active_producer_count"].as_uint64() * 10000 + gs4["standby_slot_weight"].as_uint64() * gs4["num_standby_slots"].as_uint64() );
   const double voter_fraction = 0.4;
   const double useconds_per_year = 52.0 * 7 * 24 * 3600 * 1e6;
   auto fill_bound = [&]( double fraction ) -> int64_t {
      const auto last_fill = get_global_state()["last_pervote_bucket_fill"].as<time_point>();
      const auto gap_us    = ( control->head_block_time() + fc::milliseconds(500) - last_fill ).count();
      const double supply  = double( get_token_supply().get_amount() );
      return int64_t( fraction * 0.04879 * supply * double(gap_us) / useconds_per_year * 1.05 ) + 1;
   };
   const uint64_t wrap_guard = 1ull << 62;

   // mt19937's raw output is standardised; std::uniform_int_distribution is not, so the
   // sequence is drawn with modulo to stay identical across standard libraries.
   std::mt19937 rng( 20260918 );
   auto pick_standby = [&]( std::mt19937& r ) { return int( r() % 5 ); };
   auto pick_voter   = [&]( std::mt19937& r ) { return int( r() % 4 ); };
   auto gap_hours    = [&]( std::mt19937& r ) { return int( 1 + r() % 60 ); };
   auto coin         = [&]( std::mt19937& r ) { return int( r() % 2 ); };
   int64_t paid_to_standbys = 0, paid_to_voters = 0;
   int     standby_claims = 0, voter_claims = 0;

   for( int round = 0; round < 40; ++round ) {
      produce_block( fc::hours( gap_hours( rng ) ) );
      produce_blocks(1);
      if( coin( rng ) ) {
         const name who = standby_names[ pick_standby( rng ) ];
         const int64_t  credit_bound = fill_bound( standby_fraction );
         const uint64_t pre_bucket   = get_global_state4()["standby_bucket"].as_uint64();
         const asset    before       = get_balance( who );
         const auto     result       = push_action( who, "claimstandby"_n, mvo()("owner", who) );
         const int64_t  paid         = ( get_balance( who ) - before ).get_amount();
         const uint64_t post_bucket  = get_global_state4()["standby_bucket"].as_uint64();
         const uint64_t post_total   = get_global_state4()["total_standby_share"].as_uint64();
         if( result == success() ) { BOOST_REQUIRE_GT( paid, 0 ); ++standby_claims; }
         else {
            BOOST_REQUIRE_EQUAL( 0, paid );
            BOOST_REQUIRE( result == wasm_assert_msg("already claimed rewards within past day")
                        || result == wasm_assert_msg("no standby share to claim")
                        || result == wasm_assert_msg("no standby reward to claim") );
         }
         BOOST_REQUIRE_LT( post_bucket, wrap_guard );
         BOOST_REQUIRE_LT( post_total,  wrap_guard );
         BOOST_REQUIRE_LE( uint64_t(paid), pre_bucket + uint64_t(credit_bound) );          // never more than held + credited
         BOOST_REQUIRE_GE( post_bucket + uint64_t(paid), pre_bucket );                      // credit is never negative
         BOOST_REQUIRE_LE( post_bucket + uint64_t(paid), pre_bucket + uint64_t(credit_bound) ); // and never more than inflation allows
         paid_to_standbys += paid;
      } else {
         const name who = voters[ pick_voter( rng ) ];
         const int64_t credit_bound = fill_bound( voter_fraction );
         const int64_t pre_bucket   = get_global_state()["voters_bucket"].as<int64_t>();
         const asset   before       = get_balance( who );
         const auto    result       = push_action( who, "voterclaim"_n, mvo()("owner", who) );
         const int64_t paid         = ( get_balance( who ) - before ).get_amount();
         const int64_t post_bucket  = get_global_state()["voters_bucket"].as<int64_t>();
         if( result == success() ) { BOOST_REQUIRE_GT( paid, 0 ); ++voter_claims; }
         else {
            BOOST_REQUIRE_EQUAL( 0, paid );
            BOOST_REQUIRE( result == wasm_assert_msg("already claimed rewards within past day")
                        || result == wasm_assert_msg("no rewards available.") );
         }
         BOOST_REQUIRE_GE( pre_bucket, 0 );
         BOOST_REQUIRE_GE( post_bucket, 0 );
         BOOST_REQUIRE_LE( paid, pre_bucket + credit_bound );
         BOOST_REQUIRE_GE( post_bucket + paid, pre_bucket );
         BOOST_REQUIRE_LE( post_bucket + paid, pre_bucket + credit_bound );
         paid_to_voters += paid;
      }
   }
   // The property must have been exercised, not vacuously satisfied.
   BOOST_REQUIRE_GT( standby_claims, 5 );
   BOOST_REQUIRE_GT( voter_claims, 5 );
   BOOST_REQUIRE_GT( paid_to_standbys, 0 );
   BOOST_REQUIRE_GT( paid_to_voters, 0 );
} FC_LOG_AND_RETHROW()

// WBP-2001: allowsb had no test reference. The pair is msig-only and fails closed on both
// sides - double disallow and allow-without-disallow are refused with the message that
// says why.
BOOST_FIXTURE_TEST_CASE( standby_allow_disallow_round_trip, eosio_standby_tester ) try {
   const name eosio = config::system_account_name;
   BOOST_REQUIRE_EQUAL( error("missing authority of eosio"), push_action( alice, "disallowsb"_n, mvo()("account", alice) ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("account not exist in standby disallow list"),
                        push_action( eosio, "allowsb"_n, mvo()("account", alice) ) );
   BOOST_REQUIRE_EQUAL( success(), push_action( eosio, "disallowsb"_n, mvo()("account", alice) ) );
   BOOST_REQUIRE_EQUAL( alice, get_standby_disallow_state( alice ).owner );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("account already in standby disallow list"),
                        push_action( eosio, "disallowsb"_n, mvo()("account", alice) ) );
   BOOST_REQUIRE_EQUAL( error("missing authority of eosio"), push_action( alice, "allowsb"_n, mvo()("account", alice) ) );
   BOOST_REQUIRE_EQUAL( success(), push_action( eosio, "allowsb"_n, mvo()("account", alice) ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("account not exist in standby disallow list"),
                        push_action( eosio, "allowsb"_n, mvo()("account", alice) ) );
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
