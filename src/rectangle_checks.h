#ifndef SILLY_SURVIVORS_RECTANGLE_CHECKS_H
#define SILLY_SURVIVORS_RECTANGLE_CHECKS_H

#include "packer.h"

inline bool containable(const rectangle& small, const rectangle& big) {
  return (small.x >= big.x && small.y >= big.y &&
          (small.x + small.width <= big.x + big.width) &&
          (small.y + small.height <= big.y + big.height));
}

inline bool is_overlapping(const rectangle& one, const rectangle& two) {
  return !((one.x >= two.x + two.width) || (one.x + one.width <= two.x) ||
           (one.y >= two.y + two.height) || (one.y + one.height <= two.y));
}

#endif