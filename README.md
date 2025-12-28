# Crypto Project

## Requirements
- CMake ≥ 3.20
- C++20 compiler
- Git (with submodules)

## Clone the repository
```bash
git clone --recursive https://github.com/Olly60/crypto.git
```

## Build (Release):
### Configure project in 'build-release' with Release settings and vcpkg
```cmake
cmake -S . -B build-release \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_TOOLCHAIN_FILE=./external/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Build the project using the generated build files
```cmake
cmake --build build-release
```