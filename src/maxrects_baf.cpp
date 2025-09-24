#include "packer.h"
#include <algorithm>
#include <cstdint>

using rectangle_vector = std::vector<rectangle>;
using std::uint32_t;

constexpr int invalid = -1;

rectangle make_invalid_rectangle() {
  return {invalid, invalid, invalid, invalid};
}

bool is_invalid_rectangle(rectangle r) {
  if (r.height == invalid && r.width == invalid && r.x == invalid &&
      r.y == invalid)
    return true;
  return false;
}

struct baf_score {
  int index = invalid;
  uint32_t area_fit = 0;
  uint32_t short_side_fit = 0;
  uint32_t long_side_fit = 0;
};

/* we can guarantee these casts because we'll establish a
 * predicate that this code always takes in positive ints
 * and no funky negative width, height images will be there */
rectangle img2rect(image<int> image) {
  return {image.width, image.height, 0, 0};
}

bool can_fit(const rectangle& small, const rectangle& big) {
  return small.width <= big.width && small.height <= big.height;
}

uint32_t area(const rectangle& rect) { return rect.width * rect.height; }

int break_long_tie(std::vector<baf_score>& ties) {
  // sort by long side and break it if possible
  std::sort(ties.begin(), ties.end(),
            [](const baf_score& s1, const baf_score& s2) {
              return s1.long_side_fit < s2.long_side_fit;
            });

  std::vector<baf_score> long_tie;
  uint32_t smallest = ties[0].long_side_fit;
  for (int i = 1; i < ties.size(); i++) {
    if (ties[i].long_side_fit == smallest)
      long_tie.push_back(ties[i]);
  }
  if (long_tie.size() == 0)
    return ties[0].index;

  return invalid;
}

int break_short_tie(std::vector<baf_score>& ties) {
  // sort by short side and break it if possible
  std::sort(ties.begin(), ties.end(),
            [](const baf_score& s1, const baf_score& s2) {
              return s1.short_side_fit < s2.short_side_fit;
            });

  std::vector<baf_score> short_tie;
  uint32_t smallest = ties[0].short_side_fit;
  for (int i = 1; i < ties.size(); i++) {
    if (ties[i].short_side_fit == smallest)
      short_tie.push_back(ties[i]);
  }
  if (short_tie.size() == 0)
    return ties[0].index;

  return break_long_tie(short_tie);
}

uint32_t select_best(std::vector<baf_score>& scores,
                     const rectangle_vector& selections) {
  // sort by area
  std::sort(scores.begin(), scores.end(),
            [](const baf_score& s1, const baf_score& s2) {
              return s1.area_fit < s2.area_fit;
            });

  std::vector<baf_score> tie;
  uint32_t smallest = scores[0].area_fit;
  for (int i = 1; i < scores.size(); i++) {
    if (scores[i].area_fit == smallest)
      tie.push_back(scores[i]);
  }

  if (tie.size() == 0)
    return scores[0].index;

  uint32_t index = break_short_tie(tie);
  if (index == invalid)
    return std::min_element(selections.begin(), selections.end(),
                            [](const rectangle& r1, const rectangle& r2) {
                              return r1.height < r2.height;
                            })
        ->height;

  return index;
}

rectangle calculate_best_area_fit(const rectangle& to_fit,
                                  const rectangle_vector& selections) {
  std::vector<baf_score> scores;
  scores.reserve(selections.size());
  for (int i = 0; i < selections.size(); i++) {
    scores[i].index = 0;
    scores[i].area_fit = area(selections[i]) - area(to_fit);
    scores[i].short_side_fit = std::min(selections[i].width - to_fit.width,
                                        selections[i].height - to_fit.height);
    scores[i].long_side_fit = std::max(selections[i].width - to_fit.width,
                                       selections[i].height - to_fit.height);
  }
  return selections[select_best(scores, selections)];
}

rectangle find_selection(const rectangle& to_fit,
                         const rectangle_vector& free) {
  rectangle_vector selections;
  for (int i = 0; i < free.size(); i++) {
    if (can_fit(to_fit, free[i])) {
      selections.push_back(free[i]);
    } // if can fit
  } // for free_recs

  if (selections.size() == 0)
    return make_invalid_rectangle();

  return calculate_best_area_fit(to_fit, selections);
}

rectangle_vector
maxrect_baf_pack_rectangles(int atlas_width, int atlas_height,
                            std::vector<image<int>> rectangles) {
  rectangle_vector free_recs = {{0, 0, atlas_width, atlas_height}};
  rectangle_vector placed;

  for (image<int>& to_fit : rectangles) {

    rectangle selection = find_selection(img2rect(to_fit), free_recs);
    if (is_invalid_rectangle(selection))
      return rectangle_vector{make_invalid_rectangle()};
    placed.push_back(selection);

  } // for to_fit input rectangles
}

atlas_properties pack_lol(std::vector<image<int>>& images) {
  /* we sort by area, in our guillotine impl it's max side up */
  std::sort(images.begin(), images.end(),
            [](const image<int>& img1, const image<int>& img2) {
              return (img1.width * img1.height) > (img2.width * img2.height);
            });

  /* magic defaults this time
   * we dont use heuristic stuff to decide, and instead
   * assume from our silly_survivors implementation `1024` as
   * a sane default */
  uint32_t atlas_width = 1024, atlas_height = 1024;
  while (true) {
    rectangle_vector placed_rectangles =
        maxrect_baf_pack_rectangles(atlas_width, atlas_height, images);
    if (is_invalid_rectangle(*placed_rectangles.begin()))
      atlas_width *= 2;
    else
      atlas_height *= 2;

    return {atlas_width, atlas_height, placed_rectangles};
  }
}