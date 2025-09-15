/* Copyright (c) silly_survivors authors
 * This file is an implementation of Guillotine strategy for
 * Rectangle Packing which is one of the ways 2D Bin Packing problem
 * can be solved. The code in this document is inspired from the
 * following sources:
 * https://raw.githubusercontent.com/rougier/freetype-gl/master/doc/RectangleBinPack.pdf
 * https://andrw.coffee/devlog/sprite_packer/
 * https://github.com/juj/RectangleBinPack/blob/master/GuillotineBinPack.cpp
 * and some youtube lectures...
 */

#include "packer.h"
#include <cstdint>
#include <vector>

using std::uint32_t;
struct rectangle {
  uint32_t x, y, width, height;
};

constexpr int invalid = -1;
using rectangle_vector = std::vector<rectangle>;

bool containable(const rectangle& small, const rectangle& big) {
  return (small.x >= big.x && small.y >= big.y &&
          (small.x + small.width <= big.x + big.width) &&
          (small.y + small.height <= big.y + big.height));
}

bool is_overlapping(const rectangle& one, const rectangle& two) {
  return !((one.x >= two.x + two.width) || (one.x + one.width <= two.x) ||
           (one.y >= two.y + two.height) || (one.y + one.height <= two.y));
}

rectangle_vector remove_overlapping(rectangle_vector free,
                                    const rectangle& rect) {
  rectangle_vector new_free = {};
  for (rectangle& free_rect : free) {
    // we're not overlapping
    if (!is_overlapping(free_rect, rect)) {
      new_free.push_back(free_rect);
      continue;
    } // if

    /* we are overlapping, and therefore we find the overlap region
     * then make four sub rectangles based on that */
    uint32_t overlap_x1 = std::max(free_rect.x, rect.x);
    uint32_t overlap_y1 = std::max(free_rect.y, rect.y);
    uint32_t overlap_x2 =
        std::min(free_rect.x + free_rect.width, rect.x + rect.width);
    uint32_t overlap_y2 =
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

rectangle_vector cleanup_splits(rectangle_vector free) {
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
  for (rectangle& rect : new_free) {
    result = remove_overlapping(result, rect);
    result.push_back(rect);
  }

  return result;
}

rectangle_vector guillotine_pack_rectangles(uint32_t atlas_width,
                                            uint32_t atlas_height,
                                            std::vector<image> rectangles) {
  rectangle_vector free_recs = {{0, 0, atlas_width, atlas_height}};
  rectangle_vector placed;

  for (image& to_fit : rectangles) {
    rectangle selection = {};
    int selection_index = invalid;

    for (int i = 0; i < free_recs.size(); i++) {
      rectangle cur = free_recs[i];
      // NOTE: to_fit.x or image.x,y actually represent width, height
      // and not co-ordinates
      if (to_fit.x < cur.width && to_fit.y < cur.height) {
        selection = cur;
        selection_index = i;
        break;
      }
    } // for int i = 0

    if (selection_index == invalid)
      continue;

    placed.push_back(selection);
    free_recs.erase(free_recs.begin() + selection_index,
                    free_recs.begin() + selection_index + 1);

    // GUILLOTINE!!! OFF WITH THEIR HEADS!!!
    rectangle right = {selection.x + to_fit.x, selection.y,
                       selection.width - to_fit.x,
                       static_cast<uint32_t>(to_fit.y)};
    rectangle bottom = {selection.x, selection.y + to_fit.y, selection.width,
                        selection.height - to_fit.y};

    if (right.width > 0 && right.height > 0)
      free_recs.push_back(right);
    if (bottom.width > 0 && bottom.height > 0)
      free_recs.push_back(bottom);

    free_recs = cleanup_splits(free_recs);

  } // for all rectangles
  return placed;
}