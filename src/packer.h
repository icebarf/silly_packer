/* Copyright (C) Amritpal Singh 2025

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
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
  unsigned char *data;
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

atlas_properties maxrects(std::vector<image<int>> &images);
atlas_properties guillotine(std::vector<image<int>> &images);

#endif