cmake_minimum_required(VERSION 3.22)

# Built through the same UNBSE_MOD_SOURCE_DIR hook UNBSE's own mod uses, against
# UNBSE's pinned, patched UE4SS source (out/ue4ss-source in the UNBSE project),
# but into a build tree of its own so UNBSE's hash-verified build is never
# reconfigured. Point UNBSE_MOD_SOURCE_DIR at this directory. See ../SOURCE.md.
set(TARGET OBRPlayableRaces)
project(${TARGET} LANGUAGES CXX)

add_library(${TARGET} SHARED dllmain.cpp)

# The UNBSE native SDK, vendored from UNBSE's include/ (the same five headers
# ship in every UNBSE package under ue4ss/Mods/UNBSE/sdk/).
target_include_directories(${TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/unbse-sdk"
)

target_compile_features(${TARGET} PRIVATE cxx_std_23)
target_compile_definitions(${TARGET} PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    # Several Unreal property headers are now forwarding includes that emit a
    # #pragma message on every translation unit that pulls them in.
    RC_UNREAL_DISABLE_PROPERTY_DEPRECATION_WARNINGS
)
target_link_libraries(${TARGET} PUBLIC UE4SS)
target_link_libraries(${TARGET} PRIVATE psapi)

if (MSVC)
    # Content-derived compiler and linker identities, as UNBSE builds its own
    # DLLs, so a rebuild from the same source is byte-comparable.
    target_compile_options(${TARGET} PRIVATE "/Brepro")
    target_link_options(${TARGET} PRIVATE "/Brepro")
endif()

set_target_properties(${TARGET} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "main"
    FOLDER "mods/OBRPlayableRaces"
)
