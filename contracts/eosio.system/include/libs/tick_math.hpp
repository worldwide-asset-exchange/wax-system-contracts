// "Copyright [2022] <alcor exchange>"
#ifndef INCLUDE_TICK_MATH_HPP_
#define INCLUDE_TICK_MATH_HPP_

#include <eosio/eosio.hpp>

#include "bit_math.hpp"
#include "constants.hpp"
/// @title Math library for computing sqrt prices from ticks and vice versa
/// @notice Computes sqrt price for ticks of size 1.0001, i.e. sqrt(1.0001^tick) as fixed point Q64.64 numbers. Supports
/// prices between 2**-64 and 2**64

namespace TickMath {
/// @dev The minimum tick that may be passed to #getSqrtRatioAtTick computed from log base 1.0001 of 2**-64
const int32_t MIN_TICK = -443636;
/// @dev The maximum tick that may be passed to #getSqrtRatioAtTick computed from log base 1.0001 of 2**64
const int32_t MAX_TICK = -MIN_TICK;

/// @dev The minimum value that can be returned from #getSqrtRatioAtTick. Equivalent to getSqrtRatioAtTick(MIN_TICK)
const uint128_t MIN_SQRT_RATIO = uint128_t(uint256("4295048017"));
/// @dev The maximum value that can be returned from #getSqrtRatioAtTick. Equivalent to getSqrtRatioAtTick(MAX_TICK)
const uint128_t MAX_SQRT_RATIO = uint128_t(uint256("79226673515401279992447579062"));

/// @notice Calculates sqrt(1.0001^tick) * 2^64
/// @dev Throws if |tick| > max tick
/// @param tick The input tick for the above formula
/// @return sqrtPriceX64 A Fixed point Q64.64 number representing the sqrt of the ratio of the two assets
/// (tokenB/tokenA) at the given tick
inline  uint128_t getSqrtRatioAtTick(int32_t tick) {
  uint256 absTick = tick < 0 ? uint256(-tick) : uint256(tick);
  eosio::check(absTick <= uint256(MAX_TICK), "TickOutOfBounds");

  uint256 ratio = (absTick & 0x1) != 0 ? uint256("0xfffcb933bd6fad37aa2d162d1a594001")
                                       : uint256("0x100000000000000000000000000000000");

  if ((absTick & 0x2) != 0)
    ratio = (ratio * uint256("0xfff97272373d413259a46990580e213a")) >> 128;
  if ((absTick & 0x4) != 0)
    ratio = (ratio * uint256("0xfff2e50f5f656932ef12357cf3c7fdcc")) >> 128;
  if ((absTick & 0x8) != 0)
    ratio = (ratio * uint256("0xffe5caca7e10e4e61c3624eaa0941cd0")) >> 128;
  if ((absTick & 0x10) != 0)
    ratio = (ratio * uint256("0xffcb9843d60f6159c9db58835c926644")) >> 128;
  if ((absTick & 0x20) != 0)
    ratio = (ratio * uint256("0xff973b41fa98c081472e6896dfb254c0")) >> 128;
  if ((absTick & 0x40) != 0)
    ratio = (ratio * uint256("0xff2ea16466c96a3843ec78b326b52861")) >> 128;
  if ((absTick & 0x80) != 0)
    ratio = (ratio * uint256("0xfe5dee046a99a2a811c461f1969c3053")) >> 128;
  if ((absTick & 0x100) != 0)
    ratio = (ratio * uint256("0xfcbe86c7900a88aedcffc83b479aa3a4")) >> 128;
  if ((absTick & 0x200) != 0)
    ratio = (ratio * uint256("0xf987a7253ac413176f2b074cf7815e54")) >> 128;
  if ((absTick & 0x400) != 0)
    ratio = (ratio * uint256("0xf3392b0822b70005940c7a398e4b70f3")) >> 128;
  if ((absTick & 0x800) != 0)
    ratio = (ratio * uint256("0xe7159475a2c29b7443b29c7fa6e889d9")) >> 128;
  if ((absTick & 0x1000) != 0)
    ratio = (ratio * uint256("0xd097f3bdfd2022b8845ad8f792aa5825")) >> 128;
  if ((absTick & 0x2000) != 0)
    ratio = (ratio * uint256("0xa9f746462d870fdf8a65dc1f90e061e5")) >> 128;
  if ((absTick & 0x4000) != 0)
    ratio = (ratio * uint256("0x70d869a156d2a1b890bb3df62baf32f7")) >> 128;
  if ((absTick & 0x8000) != 0)
    ratio = (ratio * uint256("0x31be135f97d08fd981231505542fcfa6")) >> 128;
  if ((absTick & 0x10000) != 0)
    ratio = (ratio * uint256("0x9aa508b5b7a84e1c677de54f3e99bc9")) >> 128;
  if ((absTick & 0x20000) != 0)
    ratio = (ratio * uint256("0x5d6af8dedb81196699c329225ee604")) >> 128;
  if ((absTick & 0x40000) != 0)
    ratio = (ratio * uint256("0x2216e584f5fa1ea926041bedfe98")) >> 128;
  if ((absTick & 0x80000) != 0)
    ratio = (ratio * uint256("0x48a170391f7dc42444e8fa2")) >> 128;

  if (tick > 0)
    ratio = UINT256_MAX / ratio;
  // This divides by 1<<64 rounding up to go from a Q128.128 to a Q128.64.
  // We then downcast because we know the result always fits within 128 bits due to our tick input constraint.
  // We round up in the division so getTickAtSqrtRatio of the output price is always consistent.
  uint256 prod = (ratio >> 64) + (ratio % (uint256(1) << 64) == 0 ? 0 : 1);
  eosio::check(prod <= UINT128_MAX, "Overflow");
  return uint128_t(prod);
}

/// @notice Calculates the greatest tick value such that getRatioAtTick(tick) <= ratio
/// @dev Throws in case sqrtPriceX96 < MIN_SQRT_RATIO, as MIN_SQRT_RATIO is the lowest value getRatioAtTick may
/// ever return.
/// @param sqrtPriceX64 The sqrt ratio for which to compute the tick as a Q64.64
/// @return tick The greatest tick for which the ratio is less than or equal to the input ratio
inline  int32_t getTickAtSqrtRatio(uint128_t sqrtPriceX64) {
  // second inequality must be < because the price can never reach the price at the max tick
  eosio::check(sqrtPriceX64 >= MIN_SQRT_RATIO && sqrtPriceX64 < MAX_SQRT_RATIO, "PriceOutOfBounds");
  uint256 ratio = uint256(sqrtPriceX64) << 64;

  uint256 r = ratio;
  uint256 msb = BitMath::mostSignificantBit(ratio);

  if (msb >= 128) {
    r = ratio >> static_cast<int>(msb - 127);
  } else {
    r = ratio << static_cast<int>(127 - msb);
  }

  int256 log_2 = (int256(msb) - 128) << 64;

  for (uint8_t i = 0; i < 14; ++i) {
    r = (r * r) >> 127;
    int256 f = r >> 128;
    log_2 = log_2 | static_cast<int256>(f << (63 - i));
    r >>= static_cast<int64_t>(f);
  }

  int256 log_sqrt10001 = log_2 * int256("255738958999603826347141");  // 128.128 number

  int32_t tickLow = int32_t((log_sqrt10001 - int256("3402992956809132418596140100660247210")) >> 128);
  int32_t tickHi = int32_t((log_sqrt10001 + int256("291339464771989622907027621153398088495")) >> 128);

  int32_t tick = tickLow == tickHi ? tickLow : getSqrtRatioAtTick(tickHi) <= sqrtPriceX64 ? tickHi : tickLow;
  return tick;
}
}  // namespace TickMath
#endif  // INCLUDE_TICK_MATH_HPP_
