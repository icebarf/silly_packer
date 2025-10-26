#include "header_writer.h"
#include "packer.h"

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
          .multi_argument()
          .set_default("");
  std::vector<std::string>& extra_files =
      kwarg("e,extras",
            "A comma separated list of extra files that can be embedded")
          .multi_argument()
          .set_default("");
  std::string& output_header =
      kwarg("o,out", "File name of the generated header")
          .set_default("silly_pack.h");
  std::string& spacename =
      kwarg("n,namespace",
            "Namespace string under which the symbols will be placed")
          .set_default("silly_packer");
  std::string& algorithm =
      kwarg("a,algorithm",
            "Use one of these algorithms to pack: maxrects, guillotine")
          .set_default("maxrects");
  bool& raylib_utils =
      kwarg("r,raylib", "Enable raylib utility functions").set_default(false);
  bool& generate_png =
      kwarg("p,png", "Generate an output png image").set_default(false);
  bool& duplicates =
      kwarg("d,duplicates",
            "Allow duplicate file inputs to be part of the atlas")
          .set_default(false);
  bool& debug =
      kwarg("debug", "Export extra symbols that can be used for debugging")
          .set_default(false);
};

inline std::vector<image<int>> images;
inline image<unsigned int> atlas;
inline std::vector<std::uint8_t> atlas_data;

void get_sanitized_name(std::string& output, const std::string_view& filename,
                        const bool using_namespace) {
  std::string file{filename};
  std::transform(file.begin(), file.end(), file.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  for (int i = 0; i < file.size(); i++) {
    if (i == 0 && std::isdigit(file[i])) {
      std::cerr << std::format("File '{}' cannot begin with a digit because of "
                               "internal sanitization rules.\n",
                               filename);
      exit(1);
    }
    if ((not std::isalnum(file[i]))) {
      file[i] = '_';
    }
  }
  output.swap(file);
}

void sanitize_image_filename(image<int>& img, const std::string_view& filename,
                             const bool using_namespace) {
  img.clean_filename.append([&filename, using_namespace]() {
    std::string name;
    get_sanitized_name(name, filename, using_namespace);
    return name;
  }());
}

void check_image_duplicates(const std::string_view& name, bool duplicates) {
  for (const image<int>& img : images) {
    if (img.filename.stem() == std::filesystem::path(name).stem()) {
      std::cerr << std::format("File '{}' alread loaded. Exiting\n", name);
      std::exit(1);
    }
  }
}

void load_image(const char* name, bool duplicates, const bool using_namespace) {
  // check for repeats and skip
  if (duplicates == false) {
    check_image_duplicates(name, duplicates);
  }

  image<int> img{};
  img.filename = std::filesystem::path(name).filename();
  img.fullpath = std::filesystem::path(name);
  img.data = stbi_load(name, &img.width, &img.height, &img.components_per_pixel,
                       STBIR_RGBA);
  if (img.data == nullptr) {
    std::cerr << std::format("{0}(): failed to load image: {1}: {2}\n",
                             __func__, name, stbi_failure_reason());
    std::cerr << "Exiting\n";
    std::exit(1);
  }

  sanitize_image_filename(img, img.filename.stem().string(), using_namespace);
  if (img.components_per_pixel != STBIR_RGBA) {
    std::cout << std::format(
        "image '{}': was not RGBA originally but has been converted to RGBA\n",
        img.filename.filename().string());
    img.components_per_pixel = STBIR_RGBA;
  }

  images.push_back(img);
}

void cleanup_stb_images() {
  for (image<int>& img : images)
    stbi_image_free(img.data);
}

atlas_properties
pack_images_to_rectangles(std::vector<std::string>& image_files,
                          std::string& algorithm, bool duplicates,
                          const bool using_namespace) {
  for (int i = 0; i < image_files.size(); i++) {
    load_image(image_files[i].c_str(), duplicates, using_namespace);
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

  std::cout << "Atlas Size\n";
  std::cout << atlas_p.width << "x" << atlas_p.height << '\n';

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
  const std::string atlas_structure_string{std::format(
      "inline constexpr struct atlas_info{{unsigned int width,height,"
      "components_per_pixel;}}"
      "atlas_info={{.width={},.height={},"
      ".components_per_pixel={}}};",
      atlas.width, atlas.height, atlas.components_per_pixel)};
  const std::string sprite_structure_string{
      "struct sprite_info{unsigned int x,y,width,height;};"};
  const std::string uv_structure_string{
      "struct uv_coords{float u0,v0,u1,v1;};"};

  header.write(atlas_structure_string);
  header.write(sprite_structure_string);
  header.write(uv_structure_string);
}

void generate_sprite_filename_array(header_writer& header) {
  std::string comma_separated_filename_literal_string{};
  for (int i = 0; i < images.size(); i++) {
    comma_separated_filename_literal_string.append(
        std::format("\"{}\",", images[i].filename.string()));
  }

  std::string sprite_indiced_filename_string{
      std::format("inline constexpr std::array<const char*,{}> "
                  "sprite_filenames={{{}}};",
                  images.size(), comma_separated_filename_literal_string)};

  header.write(sprite_indiced_filename_string);
}

void generate_raylib_function_defs(header_writer& header) {
  // clang-format off
  const std::string raylib_atlas_image_function_string {std::format(
    "inline Image raylib_atlas_image(){{"
      "return Image{{reinterpret_cast<void*>(const_cast<{}*>(atlas.data())),"
      "atlas_info.width,atlas_info.height,"
      "1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8}};"
    "}}", header.byte_type())
  };

  const std::string raylib_atlas_texture_function_string {
    "inline Texture2D raylib_atlas_texture(){"
      "return LoadTextureFromImage(raylib_atlas_image());"
    "}"
  };
  // clang-format on

  header.write(raylib_atlas_image_function_string);
  header.write(raylib_atlas_texture_function_string);
}

void generate_utility_functions(header_writer& header, bool debug) {

  /* unsure whether we need to (x,y)+0.5 or not to get something called
   * the 'texel', need input from K ig? also probably see the repeated
   * file-name situation and how to handle that since I will be generating
   * an enum from those names to access into sprite_info[N] */
  // clang-format off
  const std::string sprite_coord_normalize_function_string{
      "inline constexpr uv_coords normalized(const sprite_info sprite){"
      "return{sprite.x/float(atlas_info.width),sprite.y/float(atlas_info.height),"
      "(sprite.x+sprite.width)/float(atlas_info.width),"
      "(sprite.y+sprite.height)/float(atlas_info.height)}; }"};

  if(debug){
  /* Format: first: filename string literal count */
  const std::string index_by_str_function_string{std::format(
      "inline constexpr int get_sprite_index(const char* string){{"
        "const auto& silly_strlen=[](const char* str)constexpr{{"
          "unsigned int count = 0;"
          "while (*str!='\\0')++count,++str;"
          "return count;"
          "}};"
        "for(unsigned int i=0;i<{0};i++){{"
          "if(silly_strlen(string)!=silly_strlen(sprite_filenames[i]))continue;"
          "const char* tmp=sprite_filenames[i];"
          "while(*string!='\\0'&&*string==*tmp)++string,++tmp;"
          "if(static_cast<unsigned char>(*string)-static_cast<unsigned char>(*tmp)==0)return i;"
        "}}"
        "return -1;"
      "}}", images.size())};

    // clang-format on
    header.write(index_by_str_function_string);
  }
  header.write(sprite_coord_normalize_function_string);
}

void generate_variables(header_writer& header,
                        const atlas_properties& packed_data) {
  const std::string sprite_structure_array_string{std::format(
      "inline constexpr std::array<sprite_info,{}>sprites={{", images.size())};
  std::string sprite_filled_string{""};
  for (const rectangle& rect : packed_data.rectangles) {
    sprite_filled_string.append(std::format("sprite_info{{{},{},{},{}}},",
                                            rect.x, rect.y, rect.width,
                                            rect.height));
  }
  sprite_filled_string.append("};");

  std::string sprite_enum_string{"enum sprite_indices{"};
  for (int i = 0; i < images.size(); i++) {
    sprite_enum_string.append(
        std::format("{} = {},", images[i].clean_filename, i));
  }
  sprite_enum_string.append(
      std::format("min_index=0,max_index={},", images.size() - 1));
  sprite_enum_string.append("};");

  header.write(sprite_enum_string);
  header.write(sprite_structure_array_string);
  header.write(sprite_filled_string);
}

void generate_extra_filename_array(
    header_writer& header, const std::vector<std::filesystem::path> extra) {
  std::string comma_separated_filename_literal_string{};
  for (const std::filesystem::path filename : extra) {
    comma_separated_filename_literal_string.append(
        std::format("\"{}\",", filename.filename().string()));
  }

  std::string extras_filename_string{
      std::format("inline constexpr std::array<const char*,{}>"
                  "extra_filenames={{{}}};",
                  extra.size(), comma_separated_filename_literal_string)};

  header.write(extras_filename_string);
}

void generate_extra_utility_functions(header_writer& header,
                                      const std::size_t filenames_count) {
  // clang-format off
  /* Format: first: filename string literal count */
  const std::string index_by_str_function_string{std::format(
      "inline constexpr int get_extra_symbol_index(const char* string){{"
        "const auto& silly_strlen=[](const char* str)constexpr{{"
          "unsigned int count = 0;"
          "while (*str!='\\0')++count,++str;"
          "return count;"
          "}};"
        "for(unsigned int i=0;i<{0};i++){{"
          "if(silly_strlen(string)!=silly_strlen(extra_filenames[i]))continue;"
          "const char* tmp=extra_filenames[i];"
          "while(*string!='\\0'&&*string==*tmp)++string,++tmp;"
          "if(static_cast<unsigned char>(*string)-static_cast<unsigned char>(*tmp)==0)return i;"
        "}}"
        "return -1;"
      "}}", filenames_count)};
      header.write(index_by_str_function_string);
  // clang-format on
}

void generate_extra_symbol_pointer_array(header_writer& header,
                                         std::vector<std::string>& filenames) {

  const std::string extra_symbol_info_structure_string{
      "struct extra_symbol_info{const void* data; std::size_t size;};"};
  header.write(extra_symbol_info_structure_string);

  std::string comma_separated_filename_literal_string{};
  for (const std::string& file : filenames) {
    comma_separated_filename_literal_string.append(
        std::format("extra_symbol_info{{static_cast<const void*>({0}.data()),"
                    "{0}.size()}},",
                    file));
  }

  std::string extras_filename_string{
      std::format("inline constexpr std::array<extra_symbol_info,{}>"
                  "extra_symbol_table={{{}}};",
                  filenames.size(), comma_separated_filename_literal_string)};

  header.write(extras_filename_string);
}

void generate_extra_lookup_info(
    header_writer& header, std::vector<std::string>& filenames,
    std::vector<std::filesystem::path> actual_filenames) {
  generate_extra_filename_array(header, actual_filenames);
  generate_extra_utility_functions(header, filenames.size());
  generate_extra_symbol_pointer_array(header, filenames);
}

void generate_extra_files_arrays(header_writer& header,
                                 std::vector<std::string>& extras, bool debug) {
  std::vector<std::uint8_t> data;
  std::vector<std::filesystem::path> packed_files;
  std::vector<std::string> sanitized_filenames;

  /* for every filename in extra files */
  for (std::string& filename : extras) {
    /* duplication check */
    if (not packed_files.empty()) {
      for (std::filesystem::path& file : packed_files) {
        if (file == std::filesystem::path(filename).filename()) {
          std::cerr << std::format("File '{}' already embedded\nExiting\n",
                                   filename);
          std::exit(1);
        }
      }
    }
    packed_files.push_back(std::filesystem::path(filename).filename());

    std::ifstream input(filename);
    if (!input.is_open()) {
      std::cerr << std::format("{}: failed to open: reason: {}\n", filename,
                               std::strerror(errno));
      std::exit(1);
    }
    input.seekg(0, std::ios::end);
    std::size_t size = input.tellg();
    input.seekg(0, std::ios::beg);
    input.clear();

    data.resize(size);
    input.read(reinterpret_cast<char*>(data.data()), size);

    std::string sanitized;
    get_sanitized_name(sanitized,
                       std::filesystem::path(filename).filename().string(),
                       header.using_namespace());
    header.write_byte_array(sanitized, data.data(), data.size());
    sanitized_filenames.push_back(sanitized);

    input.close();
    data.clear();
  }

  if (debug)
    generate_extra_lookup_info(header, sanitized_filenames, packed_files);
}

void generate_atlas_header(header_writer& header, const packer_args& args,
                           const atlas_properties& packed_data) {
  if (not args.image_files.empty()) {
    generate_structures(header);
    if (args.debug) {
      generate_sprite_filename_array(header);
    }
    generate_utility_functions(header, args.debug);
    generate_variables(header, packed_data);

    // main atlas array
    header.write_byte_array(
        "atlas", atlas.data,
        atlas.height * atlas.width * atlas.components_per_pixel, true);
  }

  if (not args.extra_files.empty()) {
    generate_extra_files_arrays(header, args.extra_files, args.debug);
  }

  if (not args.image_files.empty()) {
    if (header.using_raylib())
      generate_raylib_function_defs(header);
  }
  std::cout << "Output Header: " << args.output_header << '\n';
}

atlas_properties operate_on_args(packer_args& args) {
  std::string filename = args.output_header;
  if (filename.empty()) {
    std::cerr << "Empty output header filename not allowed\n";
    std::exit(1);
  }

  if (not args.image_files.empty()) {
    // lower-case the argument just in case
    std::transform(args.algorithm.begin(), args.algorithm.end(),
                   args.algorithm.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    atlas_properties packed_data =
        pack_images_to_rectangles(args.image_files, args.algorithm,
                                  args.duplicates, not args.spacename.empty());

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

    packed_data.filename = args.output_header;
    return packed_data;
  }
  return {.filename = filename};
}

std::string get_guard_string(const std::string& filename,
                             bool using_namespace) {
  std::string sanitized;
  get_sanitized_name(sanitized, filename, using_namespace);
  std::transform(sanitized.begin(), sanitized.end(), sanitized.begin(),
                 [](const char c) { return std::toupper(c); });
  return std::format("SILLY_PACKER_GENERATED_{}_H", sanitized);
}

int main(int argc, char* argv[]) {
  if (argc == 1) {
    std::cerr << std::format("{}: must take in some image parameters\n",
                             argv[0]);
    exit(1);
  }
  packer_args args = argparse::parse<packer_args>(argc, argv);

  if (args.image_files.empty() && args.extra_files.empty()) {
    std::cerr << std::format("{}: no image inputs or extra files input "
                             "provided.\nPlease provide atleast one type\n",
                             argv[0]);
    return 1;
  }

  atlas_properties packed_data = operate_on_args(args);

  std::string guard =
      get_guard_string(packed_data.filename, not args.spacename.empty());
  header_writer header(packed_data.filename, "SILLY_PACKER_GENERATED_ATLAS_H",
                       args.spacename, args.raylib_utils);

  generate_atlas_header(header, args, packed_data);
  cleanup_stb_images();
}