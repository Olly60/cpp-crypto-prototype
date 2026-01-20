# Prototype Crypto Project

## NOTE:
This project is a prototype intended to demonstrate my C++ knowledge and is not production-ready.
To keep the scope manageable, it has known performance limitations, 
unhandled edge cases, and lacks several security features required for a real blockchain (e.g. network rate limiting and peer banning).
These would be addressed in a production implementation. Further details are covered in the accompanying document.


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

## Open the executable
This project has minimal front-end support and is intended to be run from the terminal. 
The second argument specifies the port to host on; if omitted, it defaults to 50000.

#### Navigate to the folder and run:

### Linux / macOS:
```bash
./crypto <port>
```

### Windows:
```shell 
crypto.exe <port>
```