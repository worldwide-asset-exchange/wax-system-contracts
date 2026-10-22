// WBP-2035: the inflation policy - annual rate and producer/voter/savings split set by
// setinflation and applied by fill_buckets together with the RNG carve-out (global5) and
// the standby split (global4). The expectations below replicate the contract's arithmetic
// step for step (same operand order in the floating-point rate formula, same integer
// floors), so every comparison is exact.

#include <boost/test/unit_test.hpp>
#include <cmath>
#include <random>

#include "eosio.rng_tester.hpp"

using namespace eosio_system;

struct eosio_inflation_tester : eosio_rng_tester {
   static constexpr double  useconds_per_year = 52.0 * 7 * 24 * 3600 * 1e6;   // the contract's 52-week year
   static constexpr int64_t rate_denominator  = 10000;

   // The contract's continuous rate for an annual rate in bps: ln(1 + r) truncated to five decimals.
   static double continuous_rate( int64_t annual_bps ) {
      return std::floor( std::log1p( double(annual_bps) / double(rate_denominator) ) * 100000.0 ) / 100000.0;
   }
   // distribute_tokens, in the contract's operand order.
   static int64_t expected_distribute( int64_t supply, uint64_t usecs, double rate ) {
      return int64_t( (rate * double(supply) * double(usecs)) / useconds_per_year );
   }
   static int64_t share( int64_t amount, uint64_t bps ) {
      return int64_t( (unsigned __int128)(amount) * bps / rate_denominator );
   }

   // One producer, one voter, the chain activated and the presses started.
   void activate_with_one_producer() {
      const asset large_asset = core_sym::from_string("80.0000");
      create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
      create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
      BOOST_REQUIRE_EQUAL( success(), regproducer( "defproducera"_n ) );
      produce_block( fc::hours(24) );
      transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL( success(), stake( "producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000") ) );
      // A voter earns vote share only when voting for min_bps_voting_reward producers (16 in
      // the tester); with one producer the voter path would otherwise never be live.
      BOOST_REQUIRE_EQUAL( success(), push_action( config::system_account_name, "setminbpvote"_n, mvo()("min", 1) ) );
      BOOST_REQUIRE_EQUAL( success(), vote( "producvotera"_n, { "defproducera"_n } ) );
      produce_blocks( 50 );
   }

   uint64_t fill_time() {
      return microseconds_since_epoch_of_iso_string( get_global_state()["last_pervote_bucket_fill"] );
   }

   struct ledger {
      int64_t  supply, saving, voters, bpay, producer, orng;
      int64_t  perblock_bucket, voters_bucket;
      uint64_t standby_bucket;
      uint64_t fill;
   };
   ledger snap() {
      const auto gs  = get_global_state();
      const auto gs4 = get_global_state4();
      return ledger{
         get_token_supply().get_amount(),
         get_balance( "eosio.saving"_n ).get_amount(),
         get_balance( "eosio.voters"_n ).get_amount(),
         get_balance( "eosio.bpay"_n ).get_amount(),
         get_balance( "defproducera"_n ).get_amount(),
         int64_t( get_treasury_balance().pool_balance ),
         gs["perblock_bucket"].as<int64_t>(),
         gs["voters_bucket"].as<int64_t>(),
         gs4["standby_bucket"].as_uint64(),
         fill_time()
      };
   }

   action_result claim_producer() {
      return push_action( "defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera") );
   }
   action_result claim_voter() {
      return push_action( "producvotera"_n, "voterclaim"_n, mvo()("owner", "producvotera") );
   }
   action_result setrngrate( uint64_t rng_rate, uint64_t max_pool_rng ) {
      return push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", max_pool_rng) );
   }
   action_result setsbratio( uint64_t ratio ) {
      return push_action( config::system_account_name, "setsbratio"_n, mvo()("ratio", ratio) );
   }
   action_result setsbslot( uint32_t num_slots ) {
      return push_action( config::system_account_name, "setsbslot"_n, mvo()("num_slots", num_slots) );
   }
};

BOOST_AUTO_TEST_SUITE(eosio_inflation_tests)

// The policy row defaults to the constants the chain ran before it existed, and a claim under
// those defaults issues and splits exactly what the old code did: 0.04879 continuous, 3/10 to
// block pay, 4/10 to voters, the remainder to savings. This is the "no configuration at
// upgrade" guarantee, stated as numbers.
BOOST_FIXTURE_TEST_CASE( defaults_are_the_legacy_constants, eosio_inflation_tester ) try {
   const auto g8 = get_global_state8();
   BOOST_REQUIRE_EQUAL( 500,  g8["annual_rate_bps"].as<int64_t>() );
   BOOST_REQUIRE_EQUAL( 3000, g8["producers_bps"].as<uint16_t>() );
   BOOST_REQUIRE_EQUAL( 4000, g8["voters_bps"].as<uint16_t>() );
   BOOST_REQUIRE_EQUAL( 3000, g8["savings_bps"].as<uint16_t>() );
   // The truncation rule reproduces the old literal bit for bit.
   BOOST_REQUIRE_EQUAL( 0.04879, continuous_rate( 500 ) );
   BOOST_REQUIRE_EQUAL( 0.09531, continuous_rate( 1000 ) );
   BOOST_REQUIRE_EQUAL( 0.0,     continuous_rate( 0 ) );

   activate_with_one_producer();
   const ledger before = snap();
   BOOST_REQUIRE_EQUAL( success(), claim_producer() );
   const ledger after = snap();

   const int64_t distributed = expected_distribute( before.supply, after.fill - before.fill, 0.04879 );
   BOOST_REQUIRE_GT( distributed, 0 );
   BOOST_REQUIRE_EQUAL( distributed, after.supply - before.supply );
   BOOST_REQUIRE_EQUAL( 0, after.orng - before.orng );                                   // rng_rate defaults to 0
   BOOST_REQUIRE_EQUAL( (distributed * 3) / 10, after.producer - before.producer );      // the old integer split, verbatim
   BOOST_REQUIRE_EQUAL( (distributed * 4) / 10, after.voters - before.voters );
   BOOST_REQUIRE_EQUAL( distributed - (distributed * 3) / 10 - (distributed * 4) / 10, after.saving - before.saving );
} FC_LOG_AND_RETHROW()

// Every guard on setinflation, by its message, plus the boundaries that must pass.
BOOST_FIXTURE_TEST_CASE( setinflation_guards, eosio_inflation_tester ) try {
   BOOST_REQUIRE_EQUAL( error("missing authority of eosio"), setinflation( 500, 3000, 4000, 3000, "alice1111111"_n ) );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("annual_rate_bps must be between 0 and 1000"), setinflation( -1,   3000, 4000, 3000 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("annual_rate_bps must be between 0 and 1000"), setinflation( 1001, 3000, 4000, 3000 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("producers_bps + voters_bps + savings_bps must equal 10000"), setinflation( 500, 3000, 4000, 2999 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("producers_bps + voters_bps + savings_bps must equal 10000"), setinflation( 500, 3000, 4000, 3001 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("producers_bps + voters_bps + savings_bps must equal 10000"), setinflation( 500, 0, 0, 0 ) );
   // A refused call leaves the row untouched.
   BOOST_REQUIRE_EQUAL( 500, get_global_state8()["annual_rate_bps"].as<int64_t>() );

   BOOST_REQUIRE_EQUAL( success(), setinflation( 0,    10000, 0, 0 ) );
   BOOST_REQUIRE_EQUAL( success(), setinflation( 1000, 0, 10000, 0 ) );
   BOOST_REQUIRE_EQUAL( success(), setinflation( 750,  0, 0, 10000 ) );
   BOOST_REQUIRE_EQUAL( success(), setinflation( 650,  2500, 2500, 5000 ) );
   const auto g8 = get_global_state8();
   BOOST_REQUIRE_EQUAL( 650,  g8["annual_rate_bps"].as<int64_t>() );
   BOOST_REQUIRE_EQUAL( 2500, g8["producers_bps"].as<uint16_t>() );
   BOOST_REQUIRE_EQUAL( 2500, g8["voters_bps"].as<uint16_t>() );
   BOOST_REQUIRE_EQUAL( 5000, g8["savings_bps"].as<uint16_t>() );

   // The other terms of the policy keep their bounds, now enforced from the same validator.
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("rng_rate must be less than 10000"),                setrngrate( 10000, 1 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("ratio cannot exceed PAY_SPLIT_SCALE (10000)"),     setsbratio( 10001 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("num_slots cannot exceed 21"),                      setsbslot( 22 ) );
} FC_LOG_AND_RETHROW()

// A change applies from the moment it is stored, never to the interval since the last fill:
// setinflation (and setrngrate) settle first. The interval before the call pays at the old
// rate and split; the interval after pays at the new ones.
BOOST_FIXTURE_TEST_CASE( setinflation_settles_before_change, eosio_inflation_tester ) try {
   activate_with_one_producer();
   BOOST_REQUIRE_EQUAL( success(), claim_producer() );   // fill clock now at this claim

   produce_block( fc::hours(10) );
   produce_blocks( 1 );
   const ledger before = snap();
   BOOST_REQUIRE_EQUAL( success(), setinflation( 1000, 5000, 2500, 2500 ) );
   const ledger settled = snap();

   // The settlement inside setinflation covered the gap at 5% and 3/10 : 4/10 : 3/10.
   BOOST_REQUIRE_GT( settled.fill, before.fill );
   const int64_t d_old = expected_distribute( before.supply, settled.fill - before.fill, 0.04879 );
   BOOST_REQUIRE_GT( d_old, 0 );
   BOOST_REQUIRE_EQUAL( d_old, settled.supply - before.supply );
   BOOST_REQUIRE_EQUAL( share( d_old, 3000 ), settled.bpay - before.bpay );
   BOOST_REQUIRE_EQUAL( share( d_old, 4000 ), settled.voters - before.voters );
   BOOST_REQUIRE_EQUAL( d_old - share( d_old, 3000 ) - share( d_old, 4000 ), settled.saving - before.saving );

   // From here on: 10% and 5000 : 2500 : 2500.
   produce_block( fc::hours(24) );
   produce_blocks( 1 );
   const ledger mid = snap();
   BOOST_REQUIRE_EQUAL( mid.fill, settled.fill );
   BOOST_REQUIRE_EQUAL( success(), claim_producer() );
   const ledger after = snap();
   const int64_t d_new = expected_distribute( mid.supply, after.fill - mid.fill, 0.09531 );
   BOOST_REQUIRE_GT( d_new, d_old );
   BOOST_REQUIRE_EQUAL( d_new, after.supply - mid.supply );
   // The single producer drains the block-pay bucket: this credit plus what the settlement
   // inside setinflation left there (the 3/10 of d_old, unclaimed until now).
   BOOST_REQUIRE_EQUAL( mid.perblock_bucket + share( d_new, 5000 ), after.producer - mid.producer );
   BOOST_REQUIRE_EQUAL( 0, after.perblock_bucket );
   BOOST_REQUIRE_EQUAL( share( d_new, 2500 ), after.voters - mid.voters );
   BOOST_REQUIRE_EQUAL( d_new - share( d_new, 5000 ) - share( d_new, 2500 ), after.saving - mid.saving );

   // setrngrate settles the same way: the interval before the call carries no carve-out.
   produce_block( fc::hours(10) );
   produce_blocks( 1 );
   const ledger pre_rng = snap();
   BOOST_REQUIRE_EQUAL( success(), setrngrate( 1000, 1'000'000'000'000'000ull ) );
   const ledger post_rng = snap();
   BOOST_REQUIRE_GT( post_rng.fill, pre_rng.fill );
   BOOST_REQUIRE_EQUAL( expected_distribute( pre_rng.supply, post_rng.fill - pre_rng.fill, 0.09531 ), post_rng.supply - pre_rng.supply );
   BOOST_REQUIRE_EQUAL( 0, post_rng.orng - pre_rng.orng );
} FC_LOG_AND_RETHROW()

// The whole composition at non-default values: the RNG carve-out off the top, the policy
// shares of the remainder, and the standby weight inside the producer share.
BOOST_FIXTURE_TEST_CASE( rng_then_shares_then_standby_weight, eosio_inflation_tester ) try {
   const uint64_t rng_rate = 1000, sb_weight = 5000;
   const uint32_t sb_slots = 2;
   BOOST_REQUIRE_EQUAL( success(), setrngrate( rng_rate, 1'000'000'000'000'000ull ) );
   BOOST_REQUIRE_EQUAL( success(), setsbratio( sb_weight ) );
   BOOST_REQUIRE_EQUAL( success(), setsbslot( sb_slots ) );
   BOOST_REQUIRE_EQUAL( success(), setinflation( 800, 5000, 2500, 2500 ) );
   activate_with_one_producer();

   const ledger before = snap();
   BOOST_REQUIRE_EQUAL( success(), claim_producer() );
   const ledger after = snap();

   const int64_t distributed = expected_distribute( before.supply, after.fill - before.fill, continuous_rate( 800 ) );
   BOOST_REQUIRE_GT( distributed, 0 );
   BOOST_REQUIRE_EQUAL( distributed, after.supply - before.supply );

   const int64_t rng_deposit = share( distributed, rng_rate );                   // headroom is unbounded here
   BOOST_REQUIRE_EQUAL( rng_deposit, after.orng - before.orng );
   const int64_t remainder = distributed - rng_deposit;
   const int64_t block_pay = share( remainder, 5000 );
   BOOST_REQUIRE_EQUAL( share( remainder, 2500 ), after.voters - before.voters );
   BOOST_REQUIRE_EQUAL( remainder - block_pay - share( remainder, 2500 ), after.saving - before.saving );

   const uint64_t apc          = get_global_state4()["active_producer_count"].as_uint64();
   const uint64_t total_weight = apc * 10000 + sb_weight * sb_slots;
   const int64_t  to_active    = int64_t( (unsigned __int128)(block_pay) * apc * 10000 / total_weight );
   BOOST_REQUIRE_EQUAL( to_active, after.producer - before.producer );
   BOOST_REQUIRE_EQUAL( uint64_t( block_pay - to_active ), after.standby_bucket - before.standby_bucket );
   BOOST_REQUIRE_EQUAL( block_pay - to_active, after.bpay - before.bpay );        // the standby part stays in eosio.bpay
} FC_LOG_AND_RETHROW()

// A zero share, or a zero rate, is a legal policy: nothing is transferred for the empty
// bucket, and every claim path still runs. Claims against an empty bucket fail closed with
// their usual message, or pay nothing.
BOOST_FIXTURE_TEST_CASE( zero_shares_and_zero_rate_keep_claims_working, eosio_inflation_tester ) try {
   // Everything to block pay: the voters bucket never fills, so a voter claim is refused.
   BOOST_REQUIRE_EQUAL( success(), setinflation( 500, 10000, 0, 0 ) );
   activate_with_one_producer();
   {
      const ledger before = snap();
      BOOST_REQUIRE_EQUAL( success(), claim_producer() );
      const ledger after = snap();
      const int64_t d = expected_distribute( before.supply, after.fill - before.fill, 0.04879 );
      BOOST_REQUIRE_GT( d, 0 );
      BOOST_REQUIRE_EQUAL( d, after.producer - before.producer );
      BOOST_REQUIRE_EQUAL( 0, after.voters - before.voters );
      BOOST_REQUIRE_EQUAL( 0, after.saving - before.saving );
      BOOST_REQUIRE_EQUAL( wasm_assert_msg("no rewards available."), claim_voter() );
   }
   // Everything to voters: the producer's claim succeeds and pays nothing.
   BOOST_REQUIRE_EQUAL( success(), setinflation( 500, 0, 10000, 0 ) );
   produce_block( fc::hours(24) );
   produce_blocks( 1 );
   {
      const ledger before = snap();
      BOOST_REQUIRE_EQUAL( success(), claim_producer() );
      const ledger after = snap();
      const int64_t d = expected_distribute( before.supply, after.fill - before.fill, 0.04879 );
      BOOST_REQUIRE_GT( d, 0 );
      // Nothing new for block pay: the claim only drains what the settlement inside
      // setinflation credited for the blocks since the previous claim, under the old policy.
      BOOST_REQUIRE_EQUAL( before.perblock_bucket, after.producer - before.producer );
      BOOST_REQUIRE_EQUAL( 0, after.perblock_bucket );
      BOOST_REQUIRE_EQUAL( d, after.voters - before.voters );
      BOOST_REQUIRE_EQUAL( success(), claim_voter() );
   }
   // Zero rate: no issuance, no transfers, claims still settle.
   BOOST_REQUIRE_EQUAL( success(), setinflation( 0, 3000, 4000, 3000 ) );
   produce_block( fc::hours(24) );
   produce_blocks( 1 );
   {
      const ledger before = snap();
      BOOST_REQUIRE_EQUAL( success(), claim_producer() );
      const ledger after = snap();
      BOOST_REQUIRE_GT( after.fill, before.fill );
      BOOST_REQUIRE_EQUAL( 0, after.supply - before.supply );
      BOOST_REQUIRE_EQUAL( before.perblock_bucket, after.producer - before.producer );
      BOOST_REQUIRE_EQUAL( 0, after.voters - before.voters );
      BOOST_REQUIRE_EQUAL( 0, after.saving - before.saving );
      BOOST_REQUIRE_EQUAL( 0, after.bpay - before.bpay );
   }
} FC_LOG_AND_RETHROW()

// Conservation (invariant I1) over a fixed-seed random walk through the policy space: for
// every settlement the tokens issued equal the RNG deposit plus the three transfers, each
// transfer is exactly its share, and the block-pay credit lands in the two buckets without
// remainder. The policy, RNG rate and standby split are redrawn every round so the
// settlement that follows combines values that have never been paired before.
BOOST_FIXTURE_TEST_CASE( conservation_over_random_policies, eosio_inflation_tester ) try {
   activate_with_one_producer();
   BOOST_REQUIRE_EQUAL( success(), claim_producer() );
   BOOST_REQUIRE_EQUAL( 0, get_balance( "eosio.fees"_n ).get_amount() );   // no fee offset in this scenario

   // mt19937's raw output is standardised; distributions are not, so draw with modulo.
   std::mt19937 rng( 20261022 );
   int rounds_with_issuance = 0;
   for( int round = 0; round < 30; ++round ) {
      const int64_t  annual   = int64_t( rng() % 1001 );
      const uint16_t p        = uint16_t( rng() % 10001 );
      const uint16_t v        = uint16_t( rng() % (10001 - p) );
      const uint16_t s        = uint16_t( 10000 - p - v );
      const uint64_t rng_rate = rng() % 10000;
      const uint64_t weight   = rng() % 10001;
      const uint32_t slots    = uint32_t( rng() % 22 );
      BOOST_REQUIRE_EQUAL( success(), setinflation( annual, p, v, s ) );
      BOOST_REQUIRE_EQUAL( success(), setrngrate( rng_rate, 1'000'000'000'000'000ull ) );
      BOOST_REQUIRE_EQUAL( success(), setsbratio( weight ) );
      BOOST_REQUIRE_EQUAL( success(), setsbslot( slots ) );

      produce_block( fc::hours( 24 + rng() % 48 ) );
      produce_blocks( 1 );
      const ledger before = snap();
      BOOST_REQUIRE_EQUAL( success(), claim_producer() );
      const ledger after = snap();

      const int64_t issued = after.supply - before.supply;
      BOOST_REQUIRE_EQUAL( expected_distribute( before.supply, after.fill - before.fill, continuous_rate( annual ) ), issued );
      BOOST_REQUIRE_GE( issued, 0 );
      const int64_t d_orng   = after.orng   - before.orng;
      const int64_t d_saving = after.saving - before.saving;
      const int64_t d_voters = after.voters - before.voters;
      const int64_t paid     = after.producer - before.producer;
      const int64_t d_bpay   = after.bpay - before.bpay + paid;     // the account's credit, before the claim drew on it
      BOOST_REQUIRE_GE( d_orng, 0 );   BOOST_REQUIRE_GE( d_saving, 0 );
      BOOST_REQUIRE_GE( d_voters, 0 ); BOOST_REQUIRE_GE( d_bpay, 0 ); BOOST_REQUIRE_GE( paid, 0 );

      // I1: nothing issued goes unaccounted.
      BOOST_REQUIRE_EQUAL( issued, d_orng + d_saving + d_voters + d_bpay );
      // Each destination got exactly its share of the policy in force.
      BOOST_REQUIRE_EQUAL( share( issued, rng_rate ), d_orng );
      const int64_t remainder = issued - d_orng;
      BOOST_REQUIRE_EQUAL( share( remainder, p ), d_bpay );
      BOOST_REQUIRE_EQUAL( share( remainder, v ), d_voters );
      BOOST_REQUIRE_EQUAL( remainder - share( remainder, p ) - share( remainder, v ), d_saving );
      // The block-pay credit is split between the two buckets without remainder.
      const uint64_t apc          = get_global_state4()["active_producer_count"].as_uint64();
      const uint64_t total_weight = apc * 10000 + weight * slots;
      const int64_t  to_active    = int64_t( (unsigned __int128)(d_bpay) * apc * 10000 / total_weight );
      BOOST_REQUIRE_EQUAL( uint64_t( d_bpay - to_active ), after.standby_bucket - before.standby_bucket );
      BOOST_REQUIRE_EQUAL( to_active, (after.perblock_bucket - before.perblock_bucket) + paid );
      if( issued > 0 ) ++rounds_with_issuance;
   }
   BOOST_REQUIRE_GT( rounds_with_issuance, 20 );   // the property was exercised, not vacuously met
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
