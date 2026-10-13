cmake_minimum_required(VERSION 3.28)
project(DualCEditor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DUALCEDITOR_BUILD_TESTS "Build DualCEditor tests" ${PROJECT_IS_TOP_LEVEL})
option(DUALCEDITOR_WERROR "Treat own-target warnings as errors" OFF)
set(DUALCEDITOR_DUALC_DIR "" CACHE PATH
    "Local DualC tree to build against instead of the pinned fetch")

# --- Qt 6 (find_package: Qt is installed, never fetched) -------------------
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

# --- DualC (pinned; resolution: -DDUALCEDITOR_DUALC_DIR, else FetchContent) --
# The editor links DualC's host-side example libraries (field graph parser,
# field->GLSL codegen, writers), so DualC's examples must be configured. Its
# tests are not ours to build.
set(DUALC_BUILD_EXAMPLES ON  CACHE BOOL "" FORCE)
set(DUALC_BUILD_TESTS    OFF CACHE BOOL "" FORCE)

# origin/main after #52 (ABI 0.5.1), 2026-10-07.
set(DUALCEDITOR_DUALC_COMMIT 2fcb6ee12d3b375fc234e81e319d6d17cbb1f8cd)

if(DUALCEDITOR_DUALC_DIR)
  message(STATUS "DualC: local tree ${DUALCEDITOR_DUALC_DIR}")
  add_subdirectory(${DUALCEDITOR_DUALC_DIR} ${CMAKE_BINARY_DIR}/_dualc EXCLUDE_FROM_ALL)
  set(DUALCEDITOR_DUALC_SOURCE_DIR ${DUALCEDITOR_DUALC_DIR})
else()
  message(STATUS "DualC: fetching pinned commit ${DUALCEDITOR_DUALC_COMMIT}")
  include(FetchContent)
  FetchContent_Declare(dualc
    GIT_REPOSITORY https://github.com/Giacogiak/DualC.git
    GIT_TAG        ${DUALCEDITOR_DUALC_COMMIT}
    EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(dualc)
  set(DUALCEDITOR_DUALC_SOURCE_DIR ${dualc_SOURCE_DIR})
endif()

# --- Own targets -----------------------------------------------------------
function(dualceditor_target_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(DUALCEDITOR_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(DUALCEDITOR_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# Everything but main(), so tests can drive the viewport.
add_library(dualceditor_core STATIC
  src/bench.cpp
  src/bench.h
  src/graph_file.cpp
  src/graph_file.h
  src/graph_scene.cpp
  src/graph_scene.h
  src/main_window.cpp
  src/main_window.h
  src/orbit_camera.cpp
  src/objects_panel.cpp
  src/objects_panel.h
  src/orbit_camera.h
  src/pick.cpp
  src/pick.h
  src/scene_document.cpp
  src/scene_document.h
  src/parameter_panel.cpp
  src/parameter_panel.h
  src/scene.cpp
  src/scene.h
  src/snapshot.cpp
  src/snapshot.h
  src/trace_renderer.cpp
  src/trace_renderer.h
  src/viewport.cpp
  src/viewport.h)
target_include_directories(dualceditor_core PUBLIC src)
target_link_libraries(dualceditor_core PUBLIC
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
  dualc_examples_fieldglsl)
dualceditor_target_options(dualceditor_core)

add_executable(DualCEditor src/main.cpp)
target_link_libraries(DualCEditor PRIVATE dualceditor_core)
target_compile_definitions(DualCEditor PRIVATE
  DUALCEDITOR_VERSION="${PROJECT_VERSION}")
dualceditor_target_options(DualCEditor)
# The graph the viewport opens on, read from the pinned DualC tree.
qt_add_resources(DualCEditor dualc_samples
  PREFIX /samples
  BASE ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples
  FILES ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples/gyroid_box.json)
set_target_properties(DualCEditor PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(DUALCEDITOR_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
