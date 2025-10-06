#ifndef SILLY_SURVIVORS_RECTANGLE_CHECKS_H
#define SILLY_SURVIVORS_RECTANGLE_CHECKS_H

#include "packer.h"
#include <cmath>

static constexpr int invalid = -1;

inline bool containable(const rectangle& small, const rectangle& big) {
  return (small.x >= big.x && small.y >= big.y &&
          (small.x + small.width <= big.x + big.width) &&
          (small.y + small.height <= big.y + big.height));
}

inline bool is_overlapping(const rectangle& one, const rectangle& two) {
  return !((one.x >= two.x + two.width) || (one.x + one.width <= two.x) ||
           (one.y >= two.y + two.height) || (one.y + one.height <= two.y));
}

inline rectangle make_invalid_rectangle() {
  return {invalid, invalid, invalid, invalid};
}

inline bool is_invalid_rectangle(rectangle r) {
  if (r.height == invalid && r.width == invalid && r.x == invalid &&
      r.y == invalid)
    return true;
  return false;
}

inline bool canfit(const rectangle& small, const rectangle& big) {
  return (small.width <= big.width && small.height <= big.height);
}

/* Thanks to:
 * https://graphics.stanford.edu/%7Eseander/bithacks.html#RoundUpPowerOf2
 */
inline uint32_t closest_power_of_two(uint32_t n) {
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n++;
  return n;
}

inline uint32_t calculate_min_side(const std::vector<image<int>>& images) {
  uint64_t total_area = 0;
  for (const image<int>& img : images) {
    total_area += img.width * img.height;
  }

  uint32_t minimum_side = std::ceil(std::sqrt(total_area));
  uint32_t max_rect_width = images[0].width, max_rect_height = images[0].height;
  for (int i = 1; i < images.size(); i++) {
    if (max_rect_width <= images[i].width)
      max_rect_width = images[i].width;
    if (max_rect_height <= images[i].height)
      max_rect_height = images[i].height;
  }
  minimum_side =
      std::max(minimum_side, std::max(max_rect_width, max_rect_height));
  return minimum_side;
}

#endif