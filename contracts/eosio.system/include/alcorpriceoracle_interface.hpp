// "Copyright [2023] <alcor exchange>"
#ifndef INCLUDE_ALCORPRICEORACLE_INTERFACE_HPP_
#define INCLUDE_ALCORPRICEORACLE_INTERFACE_HPP_

#include <string>
#include <tuple>
#include <vector>

#include "libs/oracle.hpp"
#include "libs/tick_math.hpp"
#include "libs/helper.hpp"

namespace AlcorPriceOracle {
static constexpr eosio::name ALCOR_SWAP_ACCOUNT = eosio::name("swap.alcor");

static eosio::checksum256 makePoolKey(eosio::extended_asset tokenA, eosio::extended_asset tokenB) {
  eosio::check(tokenA.contract.value <= tokenB.contract.value, "Contracts must be sorted");
  if (tokenA.contract.value == tokenB.contract.value) {
    eosio::check(tokenA.quantity.symbol.code().raw() < tokenB.quantity.symbol.code().raw(),
                 "Symbols must be unique and sorted");
  }
  return eosio::checksum256::make_from_word_sequence<uint64_t>(
      tokenA.contract.value, tokenA.quantity.symbol.code().raw(), tokenB.contract.value,
      tokenB.quantity.symbol.code().raw());
}

struct CurrSlotS {
  uint128_t sqrtPriceX64;
  int32_t tick;
  uint32_t lastObservationTimestamp;
  uint32_t currentObservationNum;
  uint32_t maxObservationNum;
};

// scoped by self
TABLE PoolS {
  uint64_t id;
  bool active;
  eosio::extended_asset tokenA;
  eosio::extended_asset tokenB;
  uint32_t fee;
  uint8_t feeProtocol;
  int32_t tickSpacing;
  uint64_t maxLiquidityPerTick;

  CurrSlotS currSlot;
  uint128_t feeGrowthGlobalAX64;
  uint128_t feeGrowthGlobalBX64;
  eosio::asset protocolFeeA;
  eosio::asset protocolFeeB;
  uint64_t liquidity;

  uint64_t primary_key() const { return id; }

  eosio::checksum256 secondary_key() const { return makePoolKey(tokenA, tokenB); }
};
typedef eosio::multi_index<
    "pools"_n, PoolS,
    eosio::indexed_by<"bypoolkey"_n, eosio::const_mem_fun<PoolS, eosio::checksum256, &PoolS::secondary_key>>>
    pools_t;
inline pools_t pools_ = pools_t(ALCOR_SWAP_ACCOUNT, ALCOR_SWAP_ACCOUNT.value);

inline PoolS getPool(uint64_t poolId) {
  auto _pools_itr = pools_.require_find(poolId, "poolId not found");
  return *_pools_itr;
}

/// @notice Calculates time-weighted means of tick and liquidity for a given Alcor pool
/// @param poolId Id of the pool that we want to observe
/// @param secondsAgo Number of seconds in the past from which to calculate the time-weighted means
/// @return arithmeticMeanTick The arithmetic mean tick from (block.timestamp - secondsAgo) to block.timestamp
/// @return harmonicMeanLiquidity The harmonic mean liquidity from (block.timestamp - secondsAgo) to block.timestamp
inline std::tuple<int32_t, uint64_t> consult(uint64_t poolId, uint32_t secondsAgo) {
  auto pool = getPool(poolId);
  eosio::check(secondsAgo != 0, "Seconds ago cannot be zero.");

  uint32_t time = eosio::current_time_point().sec_since_epoch();
  std::vector<uint32_t> secondsAgos(2);
  secondsAgos[0] = secondsAgo;
  secondsAgos[1] = 0;

  std::vector<int64_t> tickCumulatives(2);
  std::vector<uint128_t> secondsPerLiquidityCumulativeX64s(2);
  std::tie(tickCumulatives, secondsPerLiquidityCumulativeX64s) =
        Oracle::observe(ALCOR_SWAP_ACCOUNT, pool.id, time, secondsAgos, pool.currSlot.tick, pool.liquidity);

  int64_t tickCumulativesDelta = tickCumulatives[1] - tickCumulatives[0];
  uint128_t secondsPerLiquidityCumulativesDelta =
      secondsPerLiquidityCumulativeX64s[1] - secondsPerLiquidityCumulativeX64s[0];

  int32_t arithmeticMeanTick = int32_t(tickCumulativesDelta / secondsAgo);
  // Always round to negative infinity
  if (tickCumulativesDelta < 0 && (tickCumulativesDelta % secondsAgo != 0)) {
    arithmeticMeanTick--;
  }
  uint128_t secondsAgoX64 = uint128_t(secondsAgo) << 64;
  uint64_t harmonicMeanLiquidity = uint64_t(secondsAgoX64 / secondsPerLiquidityCumulativesDelta);
  return {tickCumulativesDelta, harmonicMeanLiquidity};
}

/// @notice Given a pool, it returns the number of seconds ago of the oldest stored observation
/// @param pool Id of AlcorPriceOracle pool that we want to observe
/// @return secondsAgo The number of seconds ago of the oldest observation stored for the pool
inline uint32_t getOldestObservationSecondsAgo(uint64_t poolId) {
  auto pool = getPool(poolId);
  Oracle::observations_t observations_table(ALCOR_SWAP_ACCOUNT, poolId);
  eosio::check(pool.currSlot.currentObservationNum > 0 && observations_table.begin() != observations_table.end(),
               "no Oracle Price found");
  auto oldestObservation = observations_table.begin();

  int32_t time = eosio::current_time_point().sec_since_epoch();
  uint32_t secondsAgo = time - oldestObservation->timestampInSec;
  return secondsAgo;
}

/**
 * @notice Converts a square root price represented in 64-bit fixed-point format to a regular price
 *         also represented in 64-bit fixed-point format.
 * @param sqrtPriceX64 The square root price represented in 64-bit fixed-point format.
 * @return The regular price represented in 64-bit fixed-point format.
 */
inline uint128_t getPriceX64FromSqrtPriceX64(uint128_t sqrtPriceX64) {
  return FullMath::mulDiv(sqrtPriceX64, sqrtPriceX64, Q64);
}

/**
 * Calculates the Time-Weighted Average Price (TWAP) for a given pool and time interval.
 *
 * @param poolId: The ID of the pool for which to calculate the TWAP.
 * @param twapInterval: The time interval in seconds for which to calculate the TWAP.
 *                      If the twapInterval is zero, the TWAP is calculated based on the current square root price of
 * the pool. Otherwise, the TWAP is calculated based on historical square root prices.
 *
 * @return The TWAP price in uint128_t.
 */
inline uint128_t getPriceTwapX64(uint64_t poolId, uint32_t twapInterval) {
  auto pool = getPool(poolId);
  if (twapInterval == 0) {
    return getPriceX64FromSqrtPriceX64(pool.currSlot.sqrtPriceX64);
  } else {
    uint32_t time = eosio::current_time_point().sec_since_epoch();
    std::vector<uint32_t>secondsAgos;
    secondsAgos.push_back(twapInterval);  // from (before)
    secondsAgos.push_back(0);             // to (now)
    std::vector<int64_t> tickCumulatives;
    std::vector<uint128_t> secondsPerLiquidityCumulativeX64s;
    std::tie(tickCumulatives, secondsPerLiquidityCumulativeX64s) =
        Oracle::observe(ALCOR_SWAP_ACCOUNT, pool.id, time, secondsAgos, pool.currSlot.tick, pool.liquidity);
    uint128_t sqrtPriceX64 =
        TickMath::getSqrtRatioAtTick(int32_t((tickCumulatives[1] - tickCumulatives[0]) / twapInterval));
    return getPriceX64FromSqrtPriceX64(sqrtPriceX64);
  }
}
};      // namespace AlcorPriceOracle
#endif  // INCLUDE_ALCORPRICEORACLE_INTERFACE_HPP_
