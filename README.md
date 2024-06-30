# rana

## Project structure

- `src` - engine source
- `vst` - VST wrappers for in-engine effects
- `corn` - converter tool from Renoise to in-engine music format 

## Building

Make sure to clone the repo with submodules:

```sh
git clone --recursive https://github.com/wermipls/rana
```

Aside from essential build tools, you will need LuaJIT installed with your system's package manager, all other dependencies should be vendored.

```sh
mkdir build
cd build
cmake ..

ninja
```
