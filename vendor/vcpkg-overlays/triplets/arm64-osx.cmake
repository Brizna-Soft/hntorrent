# Igual que el triplet arm64-osx de vcpkg (Mac con Apple Silicon, bibliotecas estáticas), con dos cambios:
# solo se compila la versión Release (como en los triplets de Windows) y funciona desde macOS 11.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

set(VCPKG_BUILD_TYPE release)
set(VCPKG_OSX_DEPLOYMENT_TARGET 11.0)
