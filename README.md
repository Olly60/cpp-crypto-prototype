# 🚀 Crypto

This project implements a core cryptocurrency node using modern C++20.  
It uses **CMake** for building and **vcpkg (Manifest Mode)** for cross-platform dependency management.  
All dependencies are automatically installed from `vcpkg.json`.  
It also includes a `CMakePresets.json` file so builds can be configured automatically.

---

# 📦 Prerequisites

Before building, ensure the following are installed:

## C++ Compiler (C++20 Compatible)
- GCC/G++ (Linux)
- Clang (macOS/Linux)
- Visual Studio 2022 (Windows)

## Git (Latest Stable)
Required for cloning the repository and its submodules.

## Ninja (Recommended)
Required if using the presets that specify the Ninja generator. Install via:

- Linux/macOS: `sudo apt install ninja-build` or `brew install ninja`
- Windows: available in Visual Studio or via `choco install ninja`

---

# 🛠 Step 1: Install System Build Tools (Required Once)

These packages are required so vcpkg can build dependencies such as **LevelDB**, **Libsodium**, and **Asio**.

## 🐧 Linux (Debian/Ubuntu/WSL)
```
sudo apt-get update
sudo apt-get install build-essential curl zip unzip tar
sudo apt-get install autoconf automake libtool
```

## 🍎 macOS (Homebrew)
```
brew install autoconf automake libtool
```

## 🪟 Windows (Visual Studio 2022)
Install Visual Studio 2022 with:

**Workload:**  
`Desktop development with C++`

VS provides all required compilers and tools.

---

# 📥 Step 2: Clone and Initialize vcpkg (One-Time Setup)

This project uses vcpkg as a submodule located in `external/vcpkg`.

## Clone with submodules
```
git clone --recursive <your_repo_url>
cd <ProjectName>
```

If you forgot `--recursive`:
```
git submodule update --init --recursive
```

## Bootstrap vcpkg

### Linux/macOS
```
./external/vcpkg/bootstrap-vcpkg.sh
```

### Windows (CMD/PowerShell)
```
.\external\vcpkg\bootstrap-vcpkg.bat
```

*(You only need to bootstrap once unless updating vcpkg.)*

---

# 🧱 Step 3: Configure and Build the Project with Presets

This project includes a **`CMakePresets.json`** file that configures the project for Debug or Release builds automatically, including the vcpkg toolchain.

## ✅ Option A: Command Line (Recommended)

```
# Configure using the debug preset
cmake --preset debug

# Build using the debug preset
cmake --build --preset debug
```

Or for release:

```
cmake --preset release
cmake --build --preset release
```

> The presets automatically set `CMAKE_TOOLCHAIN_FILE` to use vcpkg, so you don’t need to specify it manually.

### Optional: Specify a triplet in the preset if needed
You can add `"CMAKE_VCPKG_TARGET_TRIPLET": "x64-linux"` to the preset’s `cacheVariables` in `CMakePresets.json` for cross-compiling.

---

## 🖥 Option B: Using CLion / IDEs

1. Open the project in CLion.
2. Go to **File → Settings → Build, Execution, Deployment → CMake**.
3. Select the **build preset profiles** (these include the configure + build preset):
    - `Debug - Debug Preset` → Debug build with vcpkg
    - `Release - Release Preset` → Release build with vcpkg
4. Click **Reload CMake Project**.

> ⚠️ Do **not** select the configure-only presets (`Debug Preset` or `Release Preset`) — they are informational and do not run the build correctly.

Once CLion finishes configuring (vcpkg installs all dependencies), you can build and run the project normally.

---

# ▶️ Running the Built Executable

## Linux/macOS
```
./out/build/x64-Debug/<your_binary_name>
```

or for Release:

```
./out/build/x64-Release/<your_binary_name>
```

## Windows
```
out\build\x64-Debug\<your_binary_name>.exe
```

or for Release:

```
out\build\x64-Release\<your_binary_name>.exe
```

---

# 📂 Project Structure

```
<ProjectName>/
  src/                     Source code
  include/                 Header files
  external/vcpkg/          vcpkg submodule
  vcpkg.json               Manifest for dependencies
  CMakeLists.txt           Build configuration
  CMakePresets.json        Presets for Debug/Release builds
```

---

# 📄 Dependencies (vcpkg.json)

Dependencies are fully managed by vcpkg manifest mode.

Example:
```json
{
  "name": "crypto",
  "version": "1.0.0",
  "dependencies": [
    "leveldb",
    "libsodium",
    "asio"
  ]
}
```

No manual installation required—vcpkg handles everything.

---

# ✔️ Setup Complete

You now have a fully cross-platform, dependency-managed C++ crypto project using **CMake + vcpkg + presets**, ready for development or contribution.
