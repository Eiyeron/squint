cmake_minimum_required(VERSION 3.11)
project(squint)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)


# Raylib
find_package(raylib 4.0.0 QUIET) # QUIET or REQUIRED
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
  include(FetchContent)
  FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/4.0.0.tar.gz
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_Populate(raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
    add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
  endif()
endif()

# Raygui
add_subdirectory(extern/raygui/projects/CMake)
target_include_directories(raygui PUBLIC)

# IXWebSocket
SET(USE_ZLIB OFF CACHE BOOL "NO ZLIB")
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "Install IXWebSocket")
add_subdirectory(extern/IXWebSocket)
target_include_directories(ixwebsocket PUBLIC)


# -- Squint Aseprite extension --

set(EXTENSION_FILES "${CMAKE_CURRENT_SOURCE_DIR}/client/main.lua" "${CMAKE_CURRENT_SOURCE_DIR}/client/package.json")
message("[${EXTENSION_FILES}]")
add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_CURRENT_BINARY_DIR}/squint-client.aseprite-extension" --format=zip -- ${EXTENSION_FILES}
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/squint-client.aseprite-extension"
  DEPENDS "${EXTENSION_FILES}"
  COMMENT "Creating Aseprite extension"
)

# -- Squint --

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/raygui.cpp
  src/Upscaler.cpp
  src/AsepriteConnection.cpp
  # To keep track of them in IDEs.
  shaders/xbr-lv1.frag
  shaders/xbr-lv2.frag)

add_custom_target(asperite_extension ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/squint-client.aseprite-extension)

target_link_libraries(${PROJECT_NAME}
  raylib
  ixwebsocket
  raygui)

# Checks if OSX and links appropriate frameworks (only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()
