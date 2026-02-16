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
#include "packer.h"
#include "rectangle_checks.h"
#include <algorithm>
#include <cstdint>

using rectangle_vector = std::vector<rectangle>;
using std::uint32_t;

struct baf_score {
  int index = invalid;
  uint32_t area_fit = 0;
  uint32_t short_side_fit = 0;
  uint32_t long_side_fit = 0;
};

/* we can guarantee these casts because we'll establish a
 * predicate that this code always takes in positive ints
 * and no funky negative width, height images will be there */
static rectangle img2rect(image<int> image) {
  return {0, 0, image.width, image.height};
}

static uint32_t area(const rectangle &rect) { return rect.width * rect.height; }

static uint32_t select_best(std::vector<baf_score> &scores,
                            const rectangle_vector &selections) {
  // sort by area
  std::sort(scores.begin(), scores.end(),
            [](const baf_score &s1, const baf_score &s2) {
              return s1.area_fit < s2.area_fit;
            });

  std::vector<baf_score> tie;
  uint32_t smallest = scores[0].area_fit;
  for (const baf_score &s : scores) {
    if (s.area_fit == smallest)
      tie.push_back(s);
    else
      break;
  }

  if (tie.size() == 0)
    return scores[0].index;

  std::sort(tie.begin(), tie.end(),
            [](const baf_score &s1, const baf_score &s2) {
              if (s1.short_side_fit != s2.short_side_fit)
                return s1.short_side_fit < s2.short_side_fit;
              return s1.long_side_fit < s2.long_side_fit;
            });
  return tie[0].index;
}

static rectangle calculate_best_area_fit(const rectangle &to_fit,
                                         const rectangle_vector &selections) {
  std::vector<baf_score> scores(selections.size());
  scores.resize(selections.size());
  for (int i = 0; i < selections.size(); i++) {
    scores[i].index = i;
    scores[i].area_fit = area(selections[i]) - area(to_fit);
    scores[i].short_side_fit = std::min(selections[i].width - to_fit.width,
                                        selections[i].height - to_fit.height);
    scores[i].long_side_fit = std::max(selections[i].width - to_fit.width,
                                       selections[i].height - to_fit.height);
  }
  uint32_t index = select_best(scores, selections);
  return selections[index];
}

static rectangle find_selection(const rectangle &to_fit,
                                const rectangle_vector &free) {
  rectangle_vector selections;
  for (int i = 0; i < free.size(); i++) {
    if (canfit(to_fit, free[i])) {
      selections.push_back(free[i]);
    } // if can fit
  } // for free_recs

  if (selections.size() == 0)
    return make_invalid_rectangle();

  return calculate_best_area_fit(to_fit, selections);
}

static void handle_overlaps_and_splits(rectangle_vector &free_recs,
                                       const rectangle placed) {
  rectangle_vector new_free;

  auto push_if_valid = [&](rectangle r) {
    if (r.width > 0 && r.height > 0)
      new_free.push_back(r);
  };

  for (const rectangle &free : free_recs) {
    if (!is_overlapping(free, placed)) {
      new_free.push_back(free);
      continue;
    }

    if (placed.x > free.x) {
      push_if_valid({free.x, free.y, placed.x - free.x, free.height});
    }
    if ((placed.x + placed.width) < (free.x + free.width)) {
      push_if_valid({placed.x + placed.width, free.y,
                     (free.x + free.width) - (placed.x + placed.width),
                     free.height});
    }
    if (placed.y > free.y) {
      push_if_valid({free.x, free.y, free.width, placed.y - free.y});
    }
    if ((placed.y + placed.height) < (free.y + free.height)) {
      push_if_valid({free.x, placed.y + placed.height, free.width,
                     (free.y + free.height) - (placed.y + placed.height)});
    }
  } // for free in free recs
  free_recs.swap(new_free);
}

static void prune_free_overlapping(rectangle_vector &free_rects) {
  std::vector<bool> to_prune(free_rects.size(), false);
  for (int i = 0; i < free_rects.size(); i++) {
    for (int j = 0; j < free_rects.size(); j++) {
      if (i != j && containable(free_rects[i], free_rects[j]))
        to_prune[i] = true;
    }
  }

  rectangle_vector cleaned;
  for (int i = 0; i < free_rects.size(); i++) {
    if (to_prune[i])
      continue;
    cleaned.push_back(free_rects[i]);
  }

  free_rects.swap(cleaned);
}

static rectangle_vector
maxrect_baf_pack_rectangles(int atlas_width, int atlas_height,
                            std::vector<image<int>> rectangles) {
  rectangle_vector free_recs = {{0, 0, atlas_width, atlas_height}};
  rectangle_vector placed;

  for (image<int> &to_fit : rectangles) {

    rectangle selection = find_selection(img2rect(to_fit), free_recs);
    if (is_invalid_rectangle(selection))
      return rectangle_vector{make_invalid_rectangle()};
    placed.push_back({selection.x, selection.y, to_fit.width, to_fit.height});

    handle_overlaps_and_splits(
        free_recs,
        rectangle{selection.x, selection.y, to_fit.width, to_fit.height});
    prune_free_overlapping(free_recs);
  } // for to_fit input rectangles

  return placed;
}

atlas_properties maxrects(std::vector<image<int>> &images) {
  /* we sort by area, in our guillotine impl it's max side up */
  std::sort(images.begin(), images.end(),
            [](const image<int> &img1, const image<int> &img2) {
              return (img1.width * img1.height) > (img2.width * img2.height);
            });

  uint32_t atlas_width = closest_power_of_two(calculate_min_side(images)),
           atlas_height = closest_power_of_two(calculate_min_side(images));
  while (true) {
    rectangle_vector placed_rectangles =
        maxrect_baf_pack_rectangles(atlas_width, atlas_height, images);
    if (is_invalid_rectangle(*placed_rectangles.begin())) {
      if (atlas_width <= atlas_height)
        atlas_width *= 2;
      else
        atlas_height *= 2;

      continue;
    }

    return {atlas_width, atlas_height, placed_rectangles};
  }
}