cmake_minimum_required(VERSION 3.22)

# Built through the same UNBSE_MOD_SOURCE_DIR hook UNBSE uses, but into its own
# build tree so UNBSE's pinned, hash-verified build is never reconfigured.
set(TARGET OBRPlayableRaces)
project(${TARGET} LANGUAGES CXX)

add_library(${TARGET} SHARED src/dllmain.cpp)

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

set_target_properties(${TARGET} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "main"
    FOLDER "mods/OBRPlayableRaces"
)
