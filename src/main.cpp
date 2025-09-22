#include "header_writer.h"
#include "packer.h"
#include <argparse/argparse.hpp>
#include <cstring>
#include <format>
#include <iostream>
#include <stb_image.h>
#include <stb_image_write.h>
#include <vector>

inline std::vector<image> images;
inline image atlas;

void load_image(const char* name) {
  image img;
  img.data =
      stbi_load(name, &img.width, &img.height, &img.components_per_pixel, 0);
  if (img.data == nullptr) {
    std::cerr << std::format("{0}(): failed to load image: {1}: {2}\n",
                             __func__, name, stbi_failure_reason());
    std::cerr << "skipping over this file...\n";
    return;
  }
  images.push_back(img);
}

void cleanup_stb_images() {
  for (image& img : images)
    stbi_image_free(img.data);
}

atlas_properties
pack_images_to_rectangles(std::vector<std::string>& image_files) {
  for (int i = 0; i < image_files.size(); i++) {
    load_image(image_files[i].c_str());
  }

  // this sorts our images vector in accordance with
  // algorithm policy and returns the atlas placement structure
  // which contains the atlas information, including a vector that
  // lines up with the sorted images vector so that the nth element
  // of images vector has information in the nth element of
  // atlas_image_placements::rectangles vector
  atlas_properties atlas_p = pack(images);

  // std::cout << "Atlas Size\n";
  // std::cout << atlas_p.width << "x" << atlas_p.height << '\n';
  // for (int i = 0; i < images.size(); i++) {
  //   std::cout << std::format("[{}, {}] - '[{}, {}]'\n", images[i].width,
  //                            images[i].height, atlas_p.rectangles[i].x,
  //                            atlas_p.rectangles[i].y);
  // }

  return atlas_p;
}

std::vector<std::uint8_t>
convert_packed_to_atlas(const atlas_properties& properties) {
  std::vector<std::uint8_t> atlas_raw_vector;
  atlas_raw_vector.resize(properties.width * properties.height *
                          images[0].components_per_pixel);

  int prev_comps_per_pixel = images[0].components_per_pixel;

  for (int i = 0; i < images.size(); i++) {
    //..
    if (properties.rectangles[i].width != images[i].width ||
        properties.rectangles[i].height != images[i].height) {
      std::cerr << "Image and Rectangle sort mismatch, cannot recover...\n";
      std::cerr << "Index: " << i << '\n';
      exit(1);
    }

    if (prev_comps_per_pixel != images[i].components_per_pixel) {
      std::cerr << "Image pixel component size mismatch\nIndex: \n" << i;
      exit(1);
    }

    std::uint8_t* index_region =
        atlas_raw_vector.data() +
        (properties.rectangles[i].y * properties.width +
         properties.rectangles[i].x) *
            images[i].components_per_pixel;

    for (int row = 0; row < properties.rectangles[i].height; row++) {
      std::memcpy(index_region +
                      (row * properties.width * images[i].components_per_pixel),
                  images[i].data +
                      (row * images[i].width * images[i].components_per_pixel),
                  properties.rectangles[i].width *
                      images[i].components_per_pixel);
    }
  }

  return atlas_raw_vector;
}

struct packer_args : public argparse::Args {
  std::vector<std::string>& image_files =
      kwarg("i,images", "A comma separated list of image files to be packed")
          .multi_argument();
  std::vector<std::string>& extra_files =
      kwarg("e,extras",
            "A comma separated list of extra files that can be embedded")
          .set_default("");
  std::string& output_header =
      kwarg("o,out", "File name of the generated header")
          .set_default("silly_atlas.h");
  std::string& spacename =
      kwarg("n,namespace",
            "Namespace string under which the symbols will be placed")
          .set_default("silly_packer");
  std::string& algorithm =
      kwarg(
          "a,algorithm",
          "Use one of these algorithms to pack: maxrects, skyline, guillotine")
          .set_default("guillotine");
  bool& gpu_optimize =
      kwarg("g,gpu_optimize",
            "Extend input images to be squares with 2^n "
            "dimensions (Generated atlas dimensions are always 2^n)")
          .set_default(false);
  bool& use_stdlib =
      kwarg("s,stdlib", "Use the stdlib defined fixed N-bit types")
          .set_default(true);
  bool& raylib_utils =
      kwarg("r,raylib", "Enable raylib utility functions").set_default(false);
};

using namespace std::string_literals;
void operate_on_args(packer_args& args) {
  if (args.algorithm != "guillotine"s) {
    std::cerr << std::format(
        "algorithm: '{}' is not supported yet\nDefaulting to 'guillotine'\n",
        args.algorithm);
  }

  if (args.extra_files.size() > 0) {
    std::cerr << "Extra files embedding not supported yet\nSkipping...\n";
  }

  if (args.raylib_utils == true) {
    std::cerr << "Raylib utils generation in header is not supported "
                 "yet\nSkippping...\n";
  }

  atlas_properties packed_data = pack_images_to_rectangles(args.image_files);

  std::vector<std::uint8_t> atlas_data = convert_packed_to_atlas(packed_data);

  atlas = {.width = static_cast<int>(packed_data.width),
           .height = static_cast<int>(packed_data.height),
           .components_per_pixel = images[0].components_per_pixel,
           .data = atlas_data.data()};
}

int main(int argc, char* argv[]) {
  if (argc == 1) {
    std::cerr << std::format("{}: must take in some image parameters\n",
                             argv[0]);
    exit(1);
  }
  packer_args args = argparse::parse<packer_args>(argc, argv);

  operate_on_args(args);

  header_writer header(args.output_header, "SILLY_PACKER_GENERATED_ATLAS_H",
                       args.use_stdlib, args.spacename);

  header.write_byte_array("atlas", atlas.data,
                          atlas.height * atlas.width *
                              atlas.components_per_pixel);
  cleanup_stb_images();
}