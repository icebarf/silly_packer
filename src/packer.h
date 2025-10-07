#ifndef SILLY_SURVIVORS_PACKER_H
#define SILLY_SURVIVORS_PACKER_H

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct rectangle {
  int x, y, width, height;
};

template <std::integral IntType> struct image {
  IntType width, height, components_per_pixel;
  unsigned char* data;
  std::string clean_filename;     // for generating enum
  std::filesystem::path filename; // for duplication check
  std::filesystem::path
      fullpath; // actual input as path, unsued: future proofing in-case needed
};

struct atlas_properties {
  std::uint32_t width;
  std::uint32_t height;
  std::vector<rectangle> rectangles;
  std::filesystem::path filename;
};

atlas_properties maxrects(std::vector<image<int>>& images);
atlas_properties guillotine(std::vector<image<int>>& images);

#endif