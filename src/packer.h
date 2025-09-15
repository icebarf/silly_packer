#ifndef SILLY_SURVIVORS_PACKER_H
#define SILLY_SURVIVORS_PACKER_H

#include <cstdint>
#include <vector>

struct rectangle {
  uint32_t x, y, width, height;
};

struct image {
  int x, y, n;
  unsigned char* data;
};

struct atlas_image_placements {
  std::uint32_t width;
  std::uint32_t height;
  std::vector<rectangle> rectangles;
};

atlas_image_placements pack(std::vector<image>& images);

#endif