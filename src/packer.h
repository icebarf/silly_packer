#ifndef SILLY_SURVIVORS_PACKER_H
#define SILLY_SURVIVORS_PACKER_H

#include <vector>

struct image {
  int x, y, n;
  unsigned char* data;
};

inline std::vector<image> images;

#endif