// "Copyright [2022] <alcor exchange>"
#ifndef INCLUDE_BIT_MATH_HPP_
#define INCLUDE_BIT_MATH_HPP_

#include <limits>
#include <eosio/eosio.hpp>
#include "constants.hpp"

namespace BitMath {
/// @notice Returns the index of the most significant bit of the number,
///     where the least significant bit is at index 0 and the most significant bit is at index 255
/// @dev The function satisfies the property:
///     x >= 2**mostSignificantBit(x) and x < 2**(mostSignificantBit(x)+1)
/// @param x the value for which to compute the most significant bit, must be greater than 0
/// @return r the index of the most significant bit
inline  uint8_t mostSignificantBit(uint256 x) {
  eosio::check(x > 0, "InvalidInput");
  uint8_t r = 0;
  if (x > uint256("0xffffffffffffffffffffffffffffffff")) {
    x >>= 128;
    r += 128;
  }

  if (x > uint256("0xffffffffffffffff")) {
    x >>= 64;
    r += 64;
  }

  if (x > uint256("0xffffffff")) {
    x >>= 32;
    r += 32;
  }

  if (x > uint256("0xffff")) {
    x >>= 16;
    r += 16;
  }

  if (x > uint256("0xff")) {
    x >>= 8;
    r += 8;
  }

  if (x > uint256("0xf")) {
    x >>= 4;
    r += 4;
  }

  if (x > uint256("0x3")) {
    x >>= 2;
    r += 2;
  }

  if (x > uint256("0x1")) {
    r += 1;
  }
  return r;
}

inline  uint8_t leastSignificantBit(uint128_t x) {
  eosio::check(x > 0, "InvalidInput");
  uint8_t r = 127;

  if ((x & std::numeric_limits<uint64_t>::max()) > 0) {
    r -= 64;
  } else {
    x >>= 64;
  }

  if ((x & std::numeric_limits<uint32_t>::max()) > 0) {
    r -= 32;
  } else {
    x >>= 32;
  }

  if ((x & std::numeric_limits<uint16_t>::max()) > 0) {
    r -= 16;
  } else {
    x >>= 16;
  }

  if ((x & std::numeric_limits<uint8_t>::max()) > 0) {
    r -= 8;
  } else {
    x >>= 8;
  }

  if ((x & 0xf) > 0) {
    r -= 4;
  } else {
    x >>= 4;
  }

  if ((x & 0x3) > 0) {
    r -= 2;
  } else {
    x >>= 2;
  }

  if ((x & 0x1) > 0) {
    r -= 1;
  }

  return r;
}
}  // namespace BitMath
#endif  // INCLUDE_BIT_MATH_HPP_"
