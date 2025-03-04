// "Copyright [2022] <alcor exchange>"
#ifndef INCLUDE_FULL_MATH_HPP_
#define INCLUDE_FULL_MATH_HPP_
#include <eosio/eosio.hpp>

namespace FullMath {
/// @notice Calculates ceil(a×b÷denominator) with full precision. Throws if result overflows a uint128 or denominator ==
/// 0
/// @param a The multiplicand
/// @param b The multiplier
/// @param denominator The divisor
/// @return result The 128-bit result
inline  uint128_t mulDivRoundingUp(uint128_t a, uint128_t b, uint128_t denominator) {
  if (denominator == 0)
    eosio::check(false, "DivideByZeroException");

  uint256 prod = uint256(a) * uint256(b);
  uint256 result = (prod % uint256(denominator)) ? prod / uint256(denominator) + 1 : prod / uint256(denominator);
  eosio::check(result <= UINT128_MAX, "ComputationOverflow");

  return uint128_t(result);
}

inline  uint64_t mulDivRoundingUp(uint64_t a, uint64_t b, uint64_t denominator) {
  if (denominator == 0)
    eosio::check(false, "DivideByZeroException");

  uint128_t prod = uint128_t(a) * uint128_t(b);
  uint128_t result = (prod % uint128_t(denominator)) ? prod / uint128_t(denominator) + 1 : prod / uint128_t(denominator);
  eosio::check(result <= UINT64_MAX, "ComputationOverflow");

  return uint64_t(result);
}

inline  uint128_t mulDiv(uint128_t a, uint128_t b, uint128_t denominator) {
  eosio::check(denominator != 0, "DivideByZeroException");

  uint256 prod = uint256(a) * uint256(b);
  uint256 result = prod / uint256(denominator);
  eosio::check(result <= UINT128_MAX, "ComputationOverflow");
  return uint128_t(result);
}

inline  uint128_t divRoundingUp(uint128_t a, uint128_t denominator) {
  eosio::check(denominator != 0, "DivideByZeroException");
  return (a % denominator) ? a / denominator + 1 : a / denominator;
}
}  // namespace FullMath
#endif  // INCLUDE_FULL_MATH_HPP_
