cmake_minimum_required (VERSION 3.8)

project ("Pumpkin" VERSION 1.0)

# So we can use raytacing symbol even though it wasn't defined in this file.
cmake_policy(SET CMP0079 NEW)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# These variables let us copy DLLs into the bin directories of the editor/bootstrap.
set(Editor_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/src/editor/Release")
set(Editor_BINARY_DIR_D "${CMAKE_CURRENT_SOURCE_DIR}/build/src/editor/Debug")
set(Bootstrap_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/src/bootstrap/Release")
set(Bootstrap_BINARY_DIR_D "${CMAKE_CURRENT_SOURCE_DIR}/build/src/bootstrap/Debug")

add_subdirectory(src)

# Set Editor as startup project.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Editor)

add_subdirectory(lib)
