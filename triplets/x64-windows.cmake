# Custom triplet overlay — pin to VS 2022 (v143) toolset.
# Prevents vcpkg from auto-detecting a newer VS installation and
# building dependencies with a mismatched STL version.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PLATFORM_TOOLSET v143)
