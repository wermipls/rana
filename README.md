# rana

## Project structure

- `src` - engine source
- `examples` - various projects demonstrating how to use the engine and its features
- `vst` - VST wrappers for in-engine effects
- `corn` - converter tool from Renoise to in-engine music format 

## Building

Make sure to clone the repo with submodules:

```sh
git clone --recursive https://github.com/wermipls/rana
```

All build dependencies should be vendored, so CMake with an appropriate toolchain (LLVM/Clang or GNU) should be enough. On Windows (MSYS2 CLANG64):

```sh
mkdir build
cd build
cmake ..

ninja
```

## Running

You can supply the project folder as the first argument when running the engine. For example:

```sh
./rana ../examples/hello
```
