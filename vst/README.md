# vst

A set of VST plugins that wrap over the in-engine effects, allowing use in a DAW.

## Building

You will need to place the `vstsdk2.4` folder in the root directory. Under MSYS2 MINGW64 or CLANG64:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```
