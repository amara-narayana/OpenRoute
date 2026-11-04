# OpenRoute — Build Instructions

## 1. Prerequisites
- **Operating System**: Windows 11 64-bit (or Windows 10 21H2+)
- **Compiler**: Visual Studio 2022 Community (MSVC v143, C++20 standard `/std:c++20`) or MinGW GCC 14+
- **Build Tools**: CMake 3.25+ and Ninja
- **Git**: 2.30+

---

## 2. Fast Command-Line Build with Ninja & MSVC

Open a PowerShell session:
```powershell
# 1. Activate Visual Studio x64 Build Environment
cmd /c "call `"${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat`" && powershell"

# 2. Configure with CMake
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 3. Compile
cmake --build build -j 2

# 4. Run Tests
ctest --test-dir build --output-on-failure
```

---

## 3. CMake Presets Support
You can also use CMake Presets directly:
```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```
