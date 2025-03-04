// "Copyright [2022] <alcor exchange>"
#ifndef INCLUDE_ORACLE_HPP_
#define INCLUDE_ORACLE_HPP_

#include <eosio/eosio.hpp>

#include "bit_math.hpp"
#include "constants.hpp"
#include "full_math.hpp"
#include "tick_math.hpp"

/// @title Oracle
/// @notice Contains functions for managing tick processes and relevant
/// calculations
namespace Oracle {
// scoped by poolId
struct [[eosio::table, eosio::contract("alcorswap")]] ObservationS {
  // the block timestamp of the observation
  uint32_t timestampInSec;
  // the tick accumulator, i.e. tick * time elapsed since the pool was first initialized
  int64_t tickCumulative;
  // the seconds per liquidity, i.e. seconds elapsed / max(1, liquidity) since the pool was first initialized
  uint128_t secondsPerLiquidityCumulativeX64;

  uint64_t primary_key() const { return uint64_t(timestampInSec); }
};
typedef eosio::multi_index<"observations"_n, ObservationS> observations_t;

/// @notice Transforms a previous observation into a new observation, given the passage of time and the current tick and
/// liquidity values
/// @dev blockTimestamp _must_ be chronologically equal to or greater than last.blockTimestamp, safe for 0 or 1
/// overflows
/// @param lastObservation The specified observation to be transformed
/// @param timestampInSec The timestamp of the new observation
/// @param tick The active tick at the time of the new observation
/// @param liquidity The total in-range liquidity at the time of the new observation
/// @return Observation The newly populated observation
inline ObservationS transform(const ObservationS& lastObservation, uint32_t timestampInSec, int32_t tick, uint64_t liquidity) {
  uint32_t delta = timestampInSec - lastObservation.timestampInSec;
  ObservationS newObservation;
  newObservation.timestampInSec = timestampInSec;
  newObservation.tickCumulative = lastObservation.tickCumulative + int64_t(tick) * delta;
  newObservation.secondsPerLiquidityCumulativeX64 =
      lastObservation.secondsPerLiquidityCumulativeX64 + ((uint128_t(delta) << 64) / (liquidity > 0 ? liquidity : 1));

  return newObservation;
}

/// @brief Initializes the oracle array by adding the first observation
/// @param code The contract's name
/// @param ram_payer The account responsible for RAM costs
/// @param poolId The ID of the pool
/// @param time The initialization timestamp
inline void initialize(eosio::name code, eosio::name ram_payer, uint64_t poolId, uint32_t time) {
  observations_t observations_table(code, poolId);
  observations_table.emplace(ram_payer, [&](auto& row) {
    row.timestampInSec = time;
    row.tickCumulative = 0;
    row.secondsPerLiquidityCumulativeX64 = 0;
  });
}

/// @brief Fetches the latest observation in the oracle array
/// @param code The contract's name
/// @param poolId The ID of the pool
/// @return The latest observation
inline ObservationS lastObservation(eosio::name code, uint64_t poolId) {
  observations_t observations_table(code, poolId);
  eosio::check(observations_table.begin() != observations_table.end(), "EmptyObservation");
  auto last = --observations_table.end();

  return *last;
}

/// @brief Writes a new observation into the oracle array
/// @param code The contract's name
/// @param ram_payer The account responsible for RAM costs
/// @param poolId The ID of the pool
/// @param timestamp The timestamp of the new observation
/// @param tick The current tick value
/// @param liquidity The total liquidity at the time of the new observation
/// @param currentObservationNum The current observation count
/// @param maxObservationNum The maximum allowed observation count
/// @return A tuple containing updated observation timestamp and observation number
inline std::tuple<uint32_t, uint32_t> write(eosio::name code, eosio::name ram_payer, uint64_t poolId, uint32_t timestamp,
                                     int32_t tick, uint64_t liquidity, uint32_t currentObservationNum,
                                     uint32_t maxObservationNum) {
  auto last = lastObservation(code, poolId);
  eosio::check(last.timestampInSec <= timestamp, "InvalidTimestamp");
  eosio::check(currentObservationNum <= maxObservationNum, "InvalidObservationNum");
  uint32_t updatedObservationTimestamp = last.timestampInSec;
  uint32_t updatedObservationNum = currentObservationNum;
  if (last.timestampInSec == timestamp) {
    return {updatedObservationTimestamp, updatedObservationNum};
  }
  if (currentObservationNum >= maxObservationNum) {
    // remove the first one
    observations_t observations_table(code, poolId);
    observations_table.erase(observations_table.begin());
  } else {
    ++updatedObservationNum;
  }
  auto updatedObservation = transform(last, timestamp, tick, liquidity);

  observations_t observations_table(code, poolId);
  observations_table.emplace(ram_payer, [&](auto& row) {
    row.timestampInSec = updatedObservation.timestampInSec;
    row.tickCumulative = updatedObservation.tickCumulative;
    row.secondsPerLiquidityCumulativeX64 = updatedObservation.secondsPerLiquidityCumulativeX64;
  });
  updatedObservationTimestamp = timestamp;
  return {updatedObservationTimestamp, updatedObservationNum};
}

/// @notice Fetches the observations before and After a given target
/// satisfied
/// @dev Assumes there is at least 1 initialized observation.
/// Used by observeSingle() to compute the counterfactual accumulator values as of a given block timestamp.
/// @param time The current block.timestamp
/// @param target The timestamp at which the reserved observation should be for
/// @param tick The active tick at the time of the returned or simulated observation
/// @param liquidity The total pool liquidity at the time of the call
/// @return beforeTarget The observation which occurred at, or before, the given timestamp
/// @return afterTarget The observation which occurred at, or after, the given timestamp
inline std::tuple<ObservationS, ObservationS> getSurroundingObservations(eosio::name code, uint64_t poolId, uint32_t time,
                                                                  uint32_t target, int32_t tick, uint64_t liquidity) {
  observations_t observations_table(code, poolId);
  auto afterTargetItr = observations_table.lower_bound(uint64_t(target));
  // ensure that the target is chronologically at or after the oldest observation
  eosio::check(afterTargetItr != observations_table.begin(), "GreatThanOldestObservation");
  auto beforeTargetItr = std::prev(afterTargetItr);
  eosio::check(beforeTargetItr != observations_table.end(), "Observation::SanityCheck");
  if (afterTargetItr == observations_table.end()) {
    auto afterTargetObser = transform(*beforeTargetItr, time, tick, liquidity);
    eosio::check(beforeTargetItr->timestampInSec != target && afterTargetObser.timestampInSec != target,
                 "Observation::SanityCheck");
    return {*beforeTargetItr, afterTargetObser};
  }
  eosio::check(beforeTargetItr->timestampInSec != target && afterTargetItr->timestampInSec != target,
               "Observation::SanityCheck");
  return {*beforeTargetItr, *afterTargetItr};
}

/// @dev Reverts if an observation at or before the desired observation timestamp does not exist.
/// 0 may be passed as `secondsAgo' to return the current cumulative values.
/// If called with a timestamp falling between two observations, returns the counterfactual accumulator values
/// at exactly the timestamp between the two observations.
/// @param time The current block timestamp
/// @param secondsAgo The amount of time to look back, in seconds, at which point to return an observation
/// @param tick The current tick
/// @param liquidity The current in-range pool liquidity
/// @return tickCumulative The tick * time elapsed since the pool was first initialized, as of `secondsAgo`
/// @return secondsPerLiquidityCumulativeX64 The time elapsed / max(1, liquidity) since the pool was first initialized,
/// as of `secondsAgo`
inline std::tuple<int64_t, uint128_t> observeSingle(eosio::name code, uint64_t poolId, uint32_t time, uint32_t secondsAgo,
                                             int32_t tick, uint64_t liquidity) {
  observations_t observations_table(code, poolId);
  eosio::check(observations_table.begin() != observations_table.end(), "EmptyObservation");

  if (secondsAgo == 0) {
    auto last = --observations_table.end();
    if (last->timestampInSec != time) {
      auto lastObservation = transform(*last, time, tick, liquidity);
      return {lastObservation.tickCumulative, lastObservation.secondsPerLiquidityCumulativeX64};
    }
    return {last->tickCumulative, last->secondsPerLiquidityCumulativeX64};
  }
  eosio::check(time >= secondsAgo, "InvalidSecondsAgo");
  uint32_t target = time - secondsAgo;
  auto atTarget = observations_table.find(target);
  if (atTarget != observations_table.end()) {
    return {atTarget->tickCumulative, atTarget->secondsPerLiquidityCumulativeX64};
  } else {
    ObservationS beforeTarget, afterTarget;
    std::tie(beforeTarget, afterTarget) = getSurroundingObservations(code, poolId, time, target, tick, liquidity);

    uint32_t observationTimeDelta = afterTarget.timestampInSec - beforeTarget.timestampInSec;
    uint32_t targetDelta = target - beforeTarget.timestampInSec;
    int64_t tickCumulative =
        beforeTarget.tickCumulative +
        ((afterTarget.tickCumulative - beforeTarget.tickCumulative) / observationTimeDelta) * targetDelta;

    uint128_t secondsPerLiquidityCumulativeX64 =
        uint128_t(beforeTarget.secondsPerLiquidityCumulativeX64 +
                  uint256(targetDelta) *
                      (afterTarget.secondsPerLiquidityCumulativeX64 - beforeTarget.secondsPerLiquidityCumulativeX64) /
                      observationTimeDelta);
    return {tickCumulative, secondsPerLiquidityCumulativeX64};
  }
}

/// @notice Returns the accumulator values as of each time seconds ago from the given time in the array of `secondsAgos`
/// @dev Reverts if `secondsAgos` > oldest observation
/// @param time The current block.timestamp
/// @param secondsAgos Each amount of time to look back, in seconds, at which point to return an observation
/// @param tick The current tick
/// @param liquidity The current in-range pool liquidity
/// @return tickCumulatives The tick * time elapsed since the pool was first initialized, as of each `secondsAgo`
/// @return secondsPerLiquidityCumulativeX64s The cumulative seconds / max(1, liquidity) since the pool was first
/// initialized, as of each `secondsAgo`
inline std::tuple<std::vector<int64_t>, std::vector<uint128_t>> observe(eosio::name code, uint64_t poolId, uint32_t time,
                                                                 std::vector<uint32_t> secondsAgos, int32_t tick,
                                                                 uint64_t liquidity) {
  std::vector<int64_t> tickCumulatives;
  std::vector<uint128_t> secondsPerLiquidityCumulativeX64s;
  for (auto i = 0; i < secondsAgos.size(); i++) {
    std::tie(tickCumulatives[i], secondsPerLiquidityCumulativeX64s[i]) =
        observeSingle(code, poolId, time, secondsAgos[i], tick, liquidity);
  }
  return {tickCumulatives, secondsPerLiquidityCumulativeX64s};
}
}  // namespace Oracle
#endif  // INCLUDE_ORACLE_HPP_
