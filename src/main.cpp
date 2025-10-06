#include "header_writer.h"
#include "packer.h"
#include "rectangle_checks.h"

#include <algorithm>
#include <argparse/argparse.hpp>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <stb_image_write.h>
#include <vector>

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
      kwarg("a,algorithm",
            "Use one of these algorithms to pack: maxrects, guillotine")
          .set_default("maxrects");
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
  bool& generate_png =
      kwarg("p,png", "Generate an output png image").set_default(false);
  bool& duplicates =
      kwarg("d,duplicates",
            "Allow duplicate file inputs to be part of the atlas")
          .set_default(false);
};

inline std::vector<image<int>> images;
inline image<unsigned int> atlas;
inline std::vector<std::uint8_t> atlas_data;

void load_image(const char* name, bool duplicates) {
  // check for repeats and skip
  if (duplicates == false) {
    for (const image<int>& img : images) {
      if (img.filename == std::filesystem::path(name).stem()) {
        std::cerr << std::format("File '{}' alread loaded. Exiting\n", name);
        std::exit(1);
      }
    }
  }

  image<int> img{};
  img.filename = std::filesystem::path(name).stem();
  img.fullpath = std::filesystem::path(name);
  img.data =
      stbi_load(name, &img.width, &img.height, &img.components_per_pixel, 0);
  if (img.data == nullptr) {
    std::cerr << std::format("{0}(): failed to load image: {1}: {2}\n",
                             __func__, name, stbi_failure_reason());
    std::cerr << "Exiting\n";
    std::exit(1);
  }
  /* push back the upper-case filename string
   * we will go off the assumption that our filenames wont have any weird
   * characters that might not be allowed as part of variable identifier
   * since this is an internal tool, i think we should be fine with this
   * implied requirement, if not, we might have some hell break loose in the
   * generated header where we use the file-name as an identifier
   * perhaps we could run a chain of sanitization steps and a backup
   * identifier string during header generation as a safe option but i'll
   * have to discuss this with K, for now ig we roll?
   */
  img.clean_filename.append([&name]() {
    std::string stem = std::filesystem::path(name).stem();
    std::transform(stem.begin(), stem.end(), stem.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    for (int i = 0; i < stem.size(); i++) {
      if (i == 0 && std::isdigit(stem[i])) {
        std::cerr << std::format(
            "Input File '{}' must not begin with a numerical digit", name);
        std::exit(1);
      }
      if (not std::isalnum(stem[i])) {
        stem[i] = '_';
      }
    }

    return stem;
  }());
  images.push_back(img);
}

void cleanup_stb_images() {
  for (image<int>& img : images)
    stbi_image_free(img.data);
}

void resize(std::vector<image<int>> images) {
  for (image<int>& img : images) {
    image<int> new_img;
    new_img.width = closest_power_of_two(img.width);
    new_img.height = closest_power_of_two(img.width);
    new_img.components_per_pixel = img.components_per_pixel;
    new_img.data = stbir_resize_uint8_srgb(
        img.data, img.width, img.height, img.width * img.components_per_pixel,
        nullptr, new_img.width, new_img.height,
        new_img.width * new_img.components_per_pixel, STBIR_RGBA);

    stbi_image_free(img.data);
    img = new_img;
  }
}

atlas_properties
pack_images_to_rectangles(std::vector<std::string>& image_files,
                          std::string& algorithm, bool gpu_optimize,
                          bool duplicates) {
  for (int i = 0; i < image_files.size(); i++) {
    load_image(image_files[i].c_str(), duplicates);
  }

  if (gpu_optimize) {
    resize(images);
  }

  /* this sorts our images vector in accordance with
   * algorithm policy and returns the atlas placement structure
   * which contains the atlas information, including a vector that
   * lines up with the sorted images vector so that the nth element
   * of images vector has information in the nth element of
   * atlas_image_placements::rectangles vector */
  atlas_properties atlas_p;
  if (algorithm == "maxrects")
    atlas_p = maxrects(images);
  else if (algorithm == "guillotine")
    atlas_p = guillotine(images);
  else {
    std::cerr << std::format("algorithm: '{}' is not valid input\n", algorithm);
    std::exit(1);
  }

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

void generate_structures(header_writer& header) {
  const std::string atlas_structure_string{
      std::format("constexpr struct atlas_info{{unsigned int width,height,"
                  "components_per_pixel;}}"
                  "atlas_info={{.width={},.height={},"
                  ".components_per_pixel={}}};",
                  atlas.width, atlas.height, atlas.components_per_pixel)};
  const std::string sprite_structure_string{
      "struct sprite_info{unsigned int x,y,width,height;};"};
  const std::string uv_structure_string{
      "struct uv_coords{float u0, v0, u1, v1;};"};

  header.write(atlas_structure_string);
  header.write(sprite_structure_string);
  header.write(uv_structure_string);
}

void generate_sprite_filename_array(header_writer& header) {
  std::string comma_separated_filename_literal_string{};
  for (int i = 0; i < images.size(); i++) {
    comma_separated_filename_literal_string.append(
        std::format("\"{}\",", images[i].filename));
  }

  std::string sprite_indiced_filename_string{std::format(
      "static constexpr const char* (sprite_filenames[{}]) = {{{}}};",
      images.size(), comma_separated_filename_literal_string)};

  header.write(sprite_indiced_filename_string);
}

void generate_raylib_function_defs(header_writer& header) {
  // clang-format off
  const std::string raylib_atlas_image_function_string {
    "Image raylib_atlas_image() {"
      "return Image{atlas,atlas_info.width,atlas_info.height,"
      "1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};"
    "}"
  };

  const std::string raylib_atlas_texture_function_string {
    "Texture2D raylib_atlas_texture() {"
      "return LoadTextureFromImage(raylib_atlas_image());"
    "}"
  };
  // clang-format on

  header.write(raylib_atlas_image_function_string);
  header.write(raylib_atlas_texture_function_string);
}

void generate_utility_functions(header_writer& header) {

  /* unsure whether we need to (x,y)+0.5 or not to get something called
   * the 'texel', need input from K ig? also probably see the repeated
   * file-name situation and how to handle that since I will be generating
   * an enum from those names to access into sprite_info[N] */
  // clang-format off
  const std::string sprite_coord_normalize_function_string{
      "inline constexpr uv_coords normalized(const sprite_info sprite){"
      "return{sprite.x/float(atlas_info.width), sprite.y/float(atlas_info.height),"
      "(sprite.x+sprite.width)/float(atlas_info.width),"
      "(sprite.y+sprite.height)/float(atlas_info.height)}; }"};

  /* Format: first: filename string literal count
   *         second: filenames string literals command separated */
  const std::string index_by_str_function_string{std::format(
      "constexpr int get_index(const char* string) {{"
        "const auto& silly_strlen = [](const char* str) constexpr {{"
          "unsigned int count = 0;"
          "while (*str != '\\0') ++count, ++str;"
          "return count;"
          "}};"
        "for (unsigned int i = 0; i < {0}; i++){{"
          "if(silly_strlen(string) != silly_strlen(sprite_filenames[i])) continue;"
          "const char* tmp = sprite_filenames[i];"
          "while(*string != '\\0' && *string == *tmp) ++string, ++tmp;"
          "if(static_cast<unsigned char>(*string) - static_cast<unsigned char>(*tmp) == 0) return i;"
        "}}"
        "return -1;"
      "}}", images.size())};

  // clang-format on

  /* we only write declarations at the top because one of them depends on
   * `atlas` array and it's better to group them together for organization */
  if (header.using_raylib()) {
    header.write("Image raylib_atlas_image();");
    header.write("Texture2D raylib_atlas_texture();");
  }

  header.write(index_by_str_function_string);
  header.write(sprite_coord_normalize_function_string);
}

void generate_variables(header_writer& header,
                        const atlas_properties& packed_data) {
  const std::string sprite_structure_array_string{std::format(
      "inline constexpr sprite_info sprites[{}]={{", images.size())};
  std::string sprite_filled_string{""};
  for (const rectangle& rect : packed_data.rectangles) {
    sprite_filled_string.append(std::format("sprite_info{{{},{},{},{}}},",
                                            rect.x, rect.y, rect.width,
                                            rect.height));
  }
  sprite_filled_string.append("};");

  std::string sprite_enum_string{"enum sprite_indices {"};
  for (int i = 0; i < images.size(); i++) {
    sprite_enum_string.append(
        std::format("{} = {},", images[i].clean_filename, i));
  }
  sprite_enum_string.append("};");

  header.write(sprite_enum_string);
  header.write(sprite_structure_array_string);
  header.write(sprite_filled_string);
}

void generate_extra_files_arrays(header_writer& header,
                                 std::vector<std::string>& extras) {
  std::vector<std::uint8_t> data;
  for (std::string& filename : extras) {
    std::ifstream input(filename);
    if (!input.is_open()) {
      std::cerr << std::format("{}: failed to open: reason: {}\n", filename,
                               std::strerror(errno));
    }
    input.seekg(0, std::ios::end);
    std::size_t size = input.tellg();
    input.seekg(0, std::ios::beg);
    input.clear();

    data.resize(size);
    input.read(reinterpret_cast<char*>(data.data()), size);

    // assume a identifier compatible name stem
    std::string stem = std::filesystem::path(filename).stem();
    std::transform(stem.begin(), stem.end(), stem.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (int i = 0; i < stem.size(); i++) {
      if (i == 0 && std::isdigit(stem[i])) {
        std::cerr << std::format("Extra Input File '{}' must not begin with a "
                                 "numerical digit\nExiting\n",
                                 filename);
        std::filesystem::remove(header.get_path());
        std::exit(1);
      }
      if (not std::isalnum(stem[i])) {
        stem[i] = '_';
      }
    }

    header.write_byte_array(stem, data.data(), data.size());

    input.close();
    data.clear();
  }
}

void generate_atlas_header(header_writer& header, const packer_args& args,
                           const atlas_properties& packed_data) {
  generate_structures(header);
  generate_sprite_filename_array(header);
  generate_utility_functions(header);
  generate_variables(header, packed_data);

  // main atlas array
  header.write_byte_array(
      "atlas", atlas.data,
      atlas.height * atlas.width * atlas.components_per_pixel, true);

  if (header.using_raylib())
    generate_raylib_function_defs(header);

  if (args.extra_files.size() > 0) {
    generate_extra_files_arrays(header, args.extra_files);
  }
  std::cout << "Output Header: " << args.output_header << '\n';
}

using namespace std::string_literals;
atlas_properties operate_on_args(packer_args& args) {
  // lower-case the argument just in case
  std::transform(args.algorithm.begin(), args.algorithm.end(),
                 args.algorithm.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  atlas_properties packed_data = pack_images_to_rectangles(
      args.image_files, args.algorithm, args.gpu_optimize, args.duplicates);

  atlas_data = convert_packed_to_atlas(packed_data);

  atlas = {.width = packed_data.width,
           .height = packed_data.height,
           .components_per_pixel =
               static_cast<unsigned int>(images[0].components_per_pixel),
           .data = atlas_data.data()};

  if (args.generate_png) {
    std::string filename = std::format(
        "{}.png", std::filesystem::path(args.output_header).stem().c_str());
    stbi_write_png(filename.c_str(), atlas.width, atlas.height,
                   atlas.components_per_pixel, atlas_data.data(),
                   atlas.width * atlas.components_per_pixel);
    std::cout << "Output png: " << filename << '\n';
  }

  return packed_data;
}

int main(int argc, char* argv[]) {
  if (argc == 1) {
    std::cerr << std::format("{}: must take in some image parameters\n",
                             argv[0]);
    exit(1);
  }
  packer_args args = argparse::parse<packer_args>(argc, argv);

  atlas_properties packed_data = operate_on_args(args);

  header_writer header(args.output_header, "SILLY_PACKER_GENERATED_ATLAS_H",
                       args.spacename, args.use_stdlib, args.raylib_utils);

  generate_atlas_header(header, args, packed_data);
  cleanup_stb_images();
}