# Continuous integration

The hosted workflows compile the library and run its tests on Linux, macOS, Windows, and
FreeBSD. Static analysis and sanitizer builds run on Linux, and Doxygen publishes the API
reference.

## Windows header isolation

`src/document.cpp` uses the Windows cryptography API for actor ID generation. It defines
`NOMINMAX` before including `<windows.h>` so the Windows `min` and `max` macros cannot replace
standard-library functions such as `std::max` and `std::numeric_limits::max`.

The MSVC job can be reproduced from a Developer PowerShell with CMake, Git, and vcpkg available:

```powershell
git submodule update --init --recursive
vcpkg install zlib
cmake -B build -DAUTOMERGE_CPP_BUILD_TESTS=ON `
    -DAUTOMERGE_CPP_BUILD_EXAMPLES=ON `
    -DAUTOMERGE_CPP_BUILD_BENCHMARKS=ON `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
