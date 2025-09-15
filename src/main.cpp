#include "packer.h"
#include <format>
#include <iostream>
#include <stb_image.h>
#include <stb_image_write.h>

void load_image(const char* name) {
  image img;
  img.data = stbi_load(name, &img.x, &img.y, &img.n, 0);
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

  cleanup();
}