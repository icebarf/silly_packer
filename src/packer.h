#ifndef SILLY_SURVIVORS_PACKER_H
#define SILLY_SURVIVORS_PACKER_H

#include <concepts>
#include <cstdint>
#include <string>
#include <vector>

struct rectangle {
  int x, y, width, height;
};

template <std::integral IntType> struct image {
  IntType width, height, components_per_pixel;
  unsigned char* data;
  std::string filename;
  std::string fullpath;
};

struct atlas_properties {
  std::uint32_t width;
  std::uint32_t height;
  std::vector<rectangle> rectangles;
};

atlas_properties maxrects(std::vector<image<int>>& images);
atlas_properties guillotine(std::vector<image<int>>& images);

#endif