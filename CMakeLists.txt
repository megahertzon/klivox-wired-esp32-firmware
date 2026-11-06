# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(klivox-wired)

# Keep the general-purpose decoder compact without changing Wi-Fi/BLE or the SDK.
idf_component_get_property(irremote_lib jorgecis__irremoteidf COMPONENT_LIB)
target_compile_options(${irremote_lib} PRIVATE -Os)
