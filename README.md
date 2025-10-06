# silly_packer

Silly Packer is a texture packing internal tool developed for silly_survivors.

## Output Header

The output header contains things in a namespace, these are the symbols
available to you with all options set to default.

- Namespace: `silly_packer`
  - Structures
    1. struct: `atlas_info`
    2. struct: `sprite_info`
    3. struct: `uv_coords`
  - Enumeration
    1. `sprite_indices` (names you can index into the `sprites` array with)
  - Variables (inline, constexpr)
    1. atlas_info: `atlas_info` variable
    1. std::array const char*  : array `sprite_filenames`
    2. std::array sprite_info  : array `sprites`
    3. std::array std::uint8_t : array `atlas`
  - Functions (inline, constexpr)
    1. `int get_index (const char*)`
    2. `uv_coords normalized(const sprite_info)`

With raylib utilities option enabled silly_packer also generates

- Namespace: `silly_packer`
  - Functions (inline)
    1. `Image raylib_atlas_image()`;
    2. `Texture2D raylib_atlas_texture()`;

If extra files are supplied to be embedded, they are also generated as
`std::array<byte_type, N> IDENTIFIER` where `IDENTIFIER` is replaced with
lowercase filename stem of the input file after sanitization. Simply consult the
header for more information as to what is inside and how it all works.

> Consider running the generated header through `clang-format`


## Dependencies

### Libraries

- [argparse](https://github.com/morrisfranken/argparse)
- [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h)
- [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h)
- [stb_image_resize2](https://github.com/nothings/stb/blob/master/stb_image_resize2.h)

### Tooling

- CMake
- C++ Compiler with C++20 support

The library dependencies are handled via cmake, and git.
Currently [`stb`](https://github.com/nothings/stb) is handled via git submodules
which is part of the main `silly_survivors` repository.

## Usage

Assuming your input image dimensions are in power of two, simply doing

```sh
./silly_packer -i image1,image2...
```

should work fine. You can fine tune the options by seeing all the available flags
using `./silly_packer -h`

> Silly packer skips over duplicates by default

### Options available

```sh
Usage: silly_packer  [options...]

Options:
      -i,--images : A comma separated list of image files to be packed [required]
      -e,--extras : A comma separated list of extra files that can be embedded 
                    [default: empty string]
         -o,--out : File name of the generated header [default: silly_atlas.h]
   -n,--namespace : Namespace string under which the symbols will be placed
                    [default: silly_packer]
   -a,--algorithm : Use one of these algorithms to pack: maxrects, guillotine
                    [default: maxrects]
-g,--gpu_optimize : Extend input images to be squares with 2^n dimensions
                    (Generated atlas dimensions are always 2^n) [default: false]
      -s,--stdlib : Use the stdlib defined fixed N-bit types [default: true]
      -r,--raylib : Enable raylib utility functions [default: false]
         -p,--png : Generate an output png image [default: false]
  -d,--duplicates : Allow duplicate file inputs to be part of the atlas
                    [default: false]
     -?,-h,--help : print help [implicit: "true", default: false]
```

### Algorithm Selection

**Default: maxrects**

For a small number of images the `guillotine` algorithm works fine.
Anything beyond that, `maxrects` is more suitable for when we have large number of images in terms of runtime and density.

**For 1200, 32x32 images:**

### Guillotine

```sh
./build/silly_packer -e=CMakeLists.txt -n=stupid -o=stupid_atlas.h -r=true     2.65s user 0.10s system 98% cpu 2.793 total
```

#### Maxrects

```sh
./build/silly_packer -e=CMakeLists.txt -n=stupid -o=stupid_atlas.h -r=true     1.03s user 0.08s system 98% cpu 1.131 total
```

| Guillotine | Maxrects |
| ---------- | -------- |
| 300        | 300      |
| ![](./images/stupid_atlas_guillotine_300.png) | ![](./images/stupid_atlas_maxrects_300.png) |
| 1200       | 1200     |
| ![](./images/stupid_atlas_guillotine_1200.png) | ![](./images/stupid_atlas_maxrects_1200.png) |