# Prototype Crypto Project

## NOTE:
This project is a prototype created to demonstrate my C++ knowledge and is not production-ready. To keep the scope manageable, it includes known performance limitations, unhandled edge cases, and omits several production-grade security features, which would be addressed in a full implementation.

The commit history has been intentionally left unrefined, as the project is not intended for long-term maintenance and is meant to reflect the incremental development process. In a production setting, the commits would be squashed and organized for clarity and maintainability.

To see an overview of this project, go to: https://docs.google.com/document/d/1Rjvty-Aji4gGOBz-VtBTbAyxt211fhRO6ZN7CGoUAg0/edit?usp=sharing

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
