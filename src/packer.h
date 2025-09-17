#ifndef SILLY_SURVIVORS_PACKER_H
#define SILLY_SURVIVORS_PACKER_H

#include <cstdint>
#include <vector>

struct rectangle {
  int x, y, width, height;
};

struct image {
  int width, height, n;
  unsigned char* data;
};

struct atlas_properties {
  std::uint32_t width;
  std::uint32_t height;
  std::vector<rectangle> rectangles;
};

atlas_properties pack(std::vector<image>& images);

#endif