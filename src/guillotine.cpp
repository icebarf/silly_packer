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

 * This file is an implementation of Guillotine strategy for
 * Rectangle Packing which is one of the ways 2D Bin Packing problem
 * can be solved. The code in this document is inspired from the
 * following sources:
 * (White-paper that discusses various strategies)
 * https://raw.githubusercontent.com/rougier/freetype-gl/master/doc/RectangleBinPack.pdf
 * (Blog that uses the technique)
 * https://andrw.coffee/devlog/sprite_packer/
 * (existing implementation)
 * https://github.com/juj/RectangleBinPack/blob/master/GuillotineBinPack.cpp
 * and some youtube videos on the topic as well as stackoverflow threads
 */
#include "packer.h"
#include "rectangle_checks.h"
#include <algorithm>
#include <cstdint>
#include <vector>

using std::uint32_t;
using rectangle_vector = std::vector<rectangle>;

static rectangle_vector handle_overlaps_and_splits(const rectangle_vector &free,
                                                   const rectangle &rect) {
  rectangle_vector new_free = {};
  for (const rectangle &free_rect : free) {
    // we're not overlapping
    if (!is_overlapping(free_rect, rect)) {
      new_free.push_back(free_rect);
      continue;
    } // if

    /* we are overlapping, and therefore we find the overlap region
     * then make four sub rectangles based on that */
    int overlap_x1 = std::max(free_rect.x, rect.x);
    int overlap_y1 = std::max(free_rect.y, rect.y);
    int overlap_x2 =
        std::min(free_rect.x + free_rect.width, rect.x + rect.width);
    int overlap_y2 =
        std::min(free_rect.y + free_rect.height, rect.y + rect.height);

    /* this also checks if there is an overlap
     * this is repeat of is_overlap() but with the max x coord and min y coord
     * of free rectangle and the current rectangle we are checking */
    if (overlap_x1 >= overlap_x2 or overlap_y1 >= overlap_y2) {
      new_free.push_back(free_rect);
      continue;
    }

    // above
    if (overlap_y2 < free_rect.y + free_rect.height)
      new_free.push_back({free_rect.x, overlap_y2, free_rect.width,
                          (free_rect.y + free_rect.height) - overlap_y2});

    // below
    if (overlap_y1 > free_rect.y)
      new_free.push_back({free_rect.x, free_rect.y, free_rect.width,
                          overlap_y1 - free_rect.y});

    // left
    if (overlap_x1 > free_rect.x)
      new_free.push_back({free_rect.x, overlap_y1, overlap_x1 - free_rect.x,
                          overlap_y2 - overlap_y1});

    // right
    if (overlap_x2 < free_rect.x + free_rect.width)
      new_free.push_back({overlap_x2, overlap_y1,
                          (free_rect.x + free_rect.width) - overlap_x2,
                          overlap_y2 - overlap_y1});
  } // for free_rect in free

  return new_free;
}

static rectangle_vector cleanup_splits(rectangle_vector free) {
  rectangle_vector new_free = {};

  for (int i = 0; i < free.size(); i++) {
    bool keep_split = true;
    for (int j = 0; j < free.size(); j++) {
      if (i != j) {
        if (containable(free[i], free[j])) {
          keep_split = false;
          break;
        } // if containable
      } // if i != j
    } // for j

    if (keep_split)
      new_free.push_back(free[i]);
  } // for i

  // overlap cleanup
  rectangle_vector result;
  for (rectangle &rect : new_free) {
    result = handle_overlaps_and_splits(result, rect);
    result.push_back(rect);
  }

  return result;
}

static rectangle_vector
guillotine_pack_rectangles(int atlas_width, int atlas_height,
                           std::vector<image<int>> rectangles) {
  rectangle_vector free_recs = {{0, 0, atlas_width, atlas_height}};
  rectangle_vector placed;

  for (image<int> &to_fit : rectangles) {
    rectangle selection = {};
    int selection_index = invalid;

    for (int i = 0; i < free_recs.size(); i++) {
      rectangle cur = free_recs[i];
      // NOTE: to_fit.x or image.x,y actually represent width, height
      // and not co-ordinates
      if (to_fit.width <= cur.width && to_fit.height <= cur.height) {
        selection = cur;
        selection_index = i;
        break;
      }
    } // for int i = 0

    if (selection_index == invalid)
      continue;

    placed.push_back({selection.x, selection.y, to_fit.width, to_fit.height});
    free_recs.erase(free_recs.begin() + selection_index,
                    free_recs.begin() + selection_index + 1);

    // GUILLOTINE!!! OFF WITH THEIR HEADS!!!
    rectangle right = {selection.x + to_fit.width, selection.y,
                       selection.width - to_fit.width, selection.height};
    rectangle bottom = {selection.x, selection.y + to_fit.height,
                        selection.width, selection.height - to_fit.height};

    if (right.width > 0 && right.height > 0)
      free_recs.push_back(right);
    if (bottom.width > 0 && bottom.height > 0)
      free_recs.push_back(bottom);

    free_recs = cleanup_splits(free_recs);

  } // for all rectangles
  return placed;
}

atlas_properties guillotine(std::vector<image<int>> &images) {
  // this sorts the images (rectangles) vector by whatever side is larger
  std::sort(images.begin(), images.end(),
            [](const image<int> &img1, const image<int> &img2) {
              return std::max(img1.width, img1.height) >
                     std::max(img2.width, img2.height);
            });

  uint32_t atlas_width = closest_power_of_two(calculate_min_side(images)),
           atlas_height = closest_power_of_two(calculate_min_side(images));

  // we will return hopefully
  while (true) {
    std::vector<rectangle> placed_rectangles =
        guillotine_pack_rectangles(atlas_width, atlas_height, images);
    if (placed_rectangles.size() == images.size())
      return {atlas_width, atlas_height, placed_rectangles};

    // grow the atlas if we can't fit
    if (atlas_width <= atlas_height)
      atlas_width *= 2;
    else
      atlas_height *= 2;
  }
}
