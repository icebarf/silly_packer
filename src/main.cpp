#include "packer.h"
#include <format>
#include <iostream>
#include <stb_image.h>
#include <stb_image_write.h>
#include <vector>

inline std::vector<image> images;

void load_image(const char* name) {
  image img;
  img.data = stbi_load(name, &img.width, &img.height, &img.n, 0);
  if (img.data == nullptr) {
    std::cerr << std::format("{0}(): failed to load image: {1}: {2}\n",
                             __func__, name, stbi_failure_reason());
    std::cerr << "skipping over this file...\n";
    return;
  }
  images.push_back(img);
}

void cleanup() {
  for (image& img : images)
    stbi_image_free(img.data);
}

int main(int argc, char* argv[]) {
  if (argc == 1) {
    std::cerr << std::format("{}: must take in some image parameters\n",
                             argv[0]);
    exit(1);
  }

  for (int i = 1; i < argc; i++) {
    load_image(argv[i]);
  }

  images.push_back({64, 64});
  images.push_back({64, 32});
  images.push_back({32, 64});
  images.push_back({40, 40});
  images.push_back({5, 10});
  int p1 = 32, p2 = 64;
  for (int i = 0; i < 10; i++) {
    images.push_back({p2, p1});
  }

  // this sorts our images vector in accordance with
  // algorithm policy and returns the atlas placement structure
  // which contains the atlas information, including a vector that
  // lines up with the sorted images vector so that the nth element
  // of images vector has information in the nth element of
  // atlas_image_placements::rectangles vector
  atlas_properties atlas_p = pack(images);

  std::cout << "Atlas Size\n";
  std::cout << atlas_p.width << "x" << atlas_p.height << '\n';
  for (int i = 0; i < images.size(); i++) {
    std::cout << std::format("[{}, {}] - '[{}, {}]'\n", images[i].width,
                             images[i].height, atlas_p.rectangles[i].x,
                             atlas_p.rectangles[i].y);
  }

  cleanup();
}