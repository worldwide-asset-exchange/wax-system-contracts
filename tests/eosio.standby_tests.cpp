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

#include "eosio.system_tester.hpp"

inline const auto DELPHI_ORACLE = "delphioracle"_n;

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

struct delphioracle_datapoint {
      uint64_t id;
      name owner;
      uint64_t value;
      uint64_t median;
      time_point timestamp;

};

FC_REFLECT(delphioracle_datapoint, (id)(owner)(value)(median)(timestamp))

struct standby_disallow_state {
  name            owner;
};

FC_REFLECT(standby_disallow_state, (owner))


using namespace eosio_system;


bool within_error(int64_t a, int64_t b, int64_t err) { return std::abs(a - b) <= err; };
bool within_one(int64_t a, int64_t b) { return within_error(a, b, 1); }
bool within_and_gte(int64_t a, int64_t b, int64_t w) { return a - b <= w && a >= b; }



struct eosio_standby_tester : eosio_system_tester {
  
  abi_serializer delphioracle_abi_ser;
  eosio_standby_tester() { 
    const asset net = core_sym::from_string("800.0000");
    const asset cpu = core_sym::from_string("800.0000");
    const std::vector<account_name> accounts = { DELPHI_ORACLE,  };
    for (const auto& v: accounts) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("100.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
    }

    produce_blocks( 2 );

    // fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);


    set_code( DELPHI_ORACLE, contracts::util::delphioracle_wasm() );
    set_abi( DELPHI_ORACLE, contracts::util::delphioracle_abi().data() );

    {
      const auto& accnt = control->db().get<account_object,by_name>( DELPHI_ORACLE );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      delphioracle_abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
    }


     base_tester::push_action(config::system_account_name, updateauth::get_name(), DELPHI_ORACLE, mvo()
      ("account", DELPHI_ORACLE.to_string())
      ("permission", name(config::active_name).to_string())
      ("parent", name(config::owner_name).to_string())
      ("auth",  authority(1, {key_weight{get_public_key(DELPHI_ORACLE, "active" ), 1}}, {
            // permission_level_weight{{config::system_account_name, config::eosio_code_name}, 1},
            permission_level_weight{{DELPHI_ORACLE, config::eosio_code_name}, 1}
        }
      ))
    );

    
  }

  void init_delphioracle_prices() {

    base_tester::push_action(DELPHI_ORACLE, "newbounty"_n, DELPHI_ORACLE, mvo()
      ("proposer", DELPHI_ORACLE)
      ("pair", mvo()
        ("name", "waxpusd")
        ("base_symbol", "8,WAXP")
        ("base_type", 4)
        ("base_contract", "")
        ("quote_symbol", "2,USD")
        ("quote_type", 1)
        ("quote_contract", "")
        ("quoted_precision", 4)
      )
    );

    std::vector<fc::variant> payload_items;
    
    auto now = control->head_block_time();


    vector<name> owners = {
      "wizardsguild"_n,
      "wax.eastern"_n,
      "alohaeosprod"_n,
      "ivote4waxusa"_n,
      "eosphereiobp"_n,
      "eosdublinwow"_n,
      "bountyblokbp"_n,
      "blocksmithio"_n,
      "liquidstudio"_n,
    };
    // insert above data points
    for (int i = 0; i < owners.size(); i++) {
      delphioracle_datapoint dp;
      dp.id = 21 + i + 1;
      dp.owner = owners[i];
      dp.value = 102;
      dp.median = 103;
      dp.timestamp = now + fc::seconds(60 * (i + 1));
      payload_items.push_back(mvo()
        ("table_name", "datapoints"_n)
        ("scope", "waxpusd"_n)
        ("row_data", fc::raw::pack(dp))
      );  
    }

    base_tester::push_action(DELPHI_ORACLE, "eosinsert"_n, config::system_account_name, mvo()
      ("payload", payload_items)
    );

  }

  standby_producer_state get_standby_producer_state(name acc) {
   vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "standbys"_n, acc);
    return fc::raw::unpack<standby_producer_state>(data);
  }

  // read datapoints from delphioracle table
  delphioracle_datapoint get_delphioracle_datapoints_table(name id) {
    vector<char> data = get_row_by_account( DELPHI_ORACLE, "waxpusd"_n, "datapoints"_n, id );
    return fc::raw::unpack<delphioracle_datapoint>(data);
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
    for (auto itr = lower; itr->t_id == table_id && itr != idx.end(); ++itr){
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
    BOOST_REQUIRE_EQUAL( name("defproducera"), producer_keys[0].producer_name );
    return producer_names;
  }

};


BOOST_AUTO_TEST_SUITE(eosio_standby_tests)

BOOST_FIXTURE_TEST_CASE(standby_config_tests, eosio_standby_tester ) try {
  const auto& gs4 = get_global_state4();
  BOOST_TEST_REQUIRE( 0 == gs4["standby_pay_ratio_numerator"].as_uint64() );
  BOOST_TEST_REQUIRE( 0 == gs4["num_standby_slots"].as_uint64() );
  BOOST_TEST_REQUIRE( 0 == gs4["total_standby_share"].as_uint64() );


  // Test invalid values for setsbratio
  BOOST_REQUIRE_EQUAL( 
    wasm_assert_msg("ratio must be between 0 and RATIO_DENOMINATOR"),
    push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", -1) )
  );

  BOOST_REQUIRE_EQUAL( 
    wasm_assert_msg("ratio must be between 0 and RATIO_DENOMINATOR"),
    push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", 10001) )
  );


  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", 5000) )
  );

  const auto& gs42 = get_global_state4();
  BOOST_TEST_REQUIRE( 5000 == gs42["standby_pay_ratio_numerator"].as_uint64() );


  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setsbslot"_n, mvo()("num_slots", 5) )
  );

  const auto& gs43 = get_global_state4();
  BOOST_TEST_REQUIRE( 5 == gs43["num_standby_slots"].as_uint64() );

} 
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(standby_disallow_tests, eosio_standby_tester ) try {
  const name alice = "alice1111112"_n;
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
  BOOST_TEST_REQUIRE( 5000 == gs4["standby_pay_ratio_numerator"].as_uint64() );
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
  BOOST_TEST_REQUIRE( 5000 == gs4["standby_pay_ratio_numerator"].as_uint64() );
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

    // saving 2/5
    BOOST_REQUIRE_EQUAL( int64_t(expected_supply_growth) - (int64_t(expected_supply_growth)/5 * 3), savings - initial_savings );

    // perblock 1/5
    const int64_t original_perblock_bucket = int64_t( double(initial_supply.get_amount()) * double(usecs_between_fills) * (cont_rate / 5.) / usecs_per_year );
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

    // saving 2/5
    BOOST_REQUIRE_EQUAL( int64_t(expected_supply_growth) - (int64_t(expected_supply_growth)/5 * 3), savings - initial_savings );

    // perblock 1/5
    const int64_t original_perblock_bucket = int64_t( double(initial_supply.get_amount()) * double(usecs_between_fills) * (cont_rate / 5.) / usecs_per_year );
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


BOOST_FIXTURE_TEST_CASE(dynamic_bp_number_test, eosio_standby_tester) try {
  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

  init_delphioracle_prices();
  produce_blocks(1);
  auto datapoints = get_delphioracle_datapoints_table(eosio::chain::name(22));
  wdump((datapoints));

  BOOST_REQUIRE_EQUAL(datapoints.value, 102);
  BOOST_REQUIRE_EQUAL(datapoints.median, 103);
  BOOST_REQUIRE_EQUAL(datapoints.owner, "wizardsguild"_n);
  
  const uint32_t USD_PER_BP = 2000;
  const uint32_t MIN_BPS = 7;
  const uint32_t MAX_BPS = 21;
  const uint32_t STANDBY_OFFSET = 3;
  const name PAIR_NAME = "waxpusd"_n;
  const uint32_t PRICE_AVERAGE_DAYS = 30;


  // set usd per bp
  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setusdbp"_n, mvo()("usd_per_bp", USD_PER_BP) )
  );

  // set min max bps
  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setbpsparams"_n, mvo()("min_bps", MIN_BPS)("max_bps", MAX_BPS)("standby_offset", STANDBY_OFFSET) )
  );

  // set delphi pair
  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setdelphipr"_n, mvo()("delphi_pair", PAIR_NAME)("price_average_days", PRICE_AVERAGE_DAYS) )
  );

  // enable dynamic bps
  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "enabledynbp"_n, mvo()("enable_dynamic_bp", true) )
  );

  produce_blocks(5);

  auto global4_state = get_global_state4();
  wdump((global4_state)); 
  BOOST_REQUIRE_EQUAL(global4_state["min_bps"].as<uint32_t>(), MIN_BPS);
  BOOST_REQUIRE_EQUAL(global4_state["max_bps"].as<uint32_t>(), MAX_BPS);
  BOOST_REQUIRE_EQUAL(global4_state["standby_offset"].as<uint32_t>(), STANDBY_OFFSET);
  BOOST_REQUIRE_EQUAL(global4_state["enable_dynamic_bp"].as<bool>(), true);
  BOOST_REQUIRE_EQUAL(global4_state["delphi_pair"].as<name>(), PAIR_NAME);
  BOOST_REQUIRE_EQUAL(global4_state["price_average_days"].as<uint32_t>(), PRICE_AVERAGE_DAYS);

  // check price average
  BOOST_REQUIRE_EQUAL(global4_state["last_average_price"].as<uint64_t>(), 103);  

  auto producer_names = active_and_vote_producers_and_standbys();
  wdump((producer_names));

  auto producer_keys = control->head_block_state()->active_schedule.producers;
  wdump((producer_keys));

  // check producers length is 17
  BOOST_TEST_REQUIRE( 17 == producer_keys.size() );

  const asset    initial_supply            = get_token_supply();
  const int64_t secs_per_year  = 52 * 7 * 24 * 3600;
  const double  usecs_per_year = secs_per_year * 1000000;
  const double secs_per_30_days = 30 * 24 * 3600;
  const double usecs_per_30_days = secs_per_30_days * 1000000;
  const double  cont_rate      = 0.04879;;

  const double expected_supply_growth_30_days = initial_supply.get_amount() * double(usecs_per_30_days) * cont_rate / usecs_per_year;
  ilog("expected_supply_growth_30_days: ${x}", ("x", expected_supply_growth_30_days));
  uint32_t wax_per_bp = USD_PER_BP * 100 * 10000; // 2000 usd with rate wax/usd = 1/100 and decimal of 4
  auto raw_producers = std::floor(expected_supply_growth_30_days / wax_per_bp);
  ilog("raw_producers: ${x}", ("x", raw_producers));

  BOOST_TEST_REQUIRE( 17 == raw_producers - STANDBY_OFFSET );

  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);

}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
