# silly_packer

Silly Packer is a texture packing internal tool developed for silly_survivors.

## Dependencies

### Libraries

- [argparse](https://github.com/morrisfranken/argparse)
- [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h)
- [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h)

### Tooling

- CMake
- C++ Compiler with C++20 support

The library dependencies are handled via cmake, and git.
Currently [`stb`](https://github.com/nothings/stb) is handled via git submodules
which is part of the main `silly_survivors` repository.

## Usage

### Algorithm Selection

**Default: Guillotine**

For a small number of images with dense packing, like with `silly_survivors`
the `guillotine` algorithm is recommended because of densely it generates the
atlas. The small number of images based on a practical guesstimate are, 600 images.
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