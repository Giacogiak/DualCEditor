cmake_minimum_required(VERSION 3.20)
project(DualCEditor
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(DUALCEDITOR_BUILD_TESTS "Build tests" OFF)
option(DUALCEDITOR_WERROR "Treat warnings as errors" OFF)

# Qt
find_package(Qt5 COMPONENTS Core Gui OpenGL Widgets QUIET)
if(NOT Qt5_FOUND)
  find_package(Qt6 COMPONENTS Core Gui OpenGL OpenGLWidgets Widgets REQUIRED)
  set(QT_FOUND TRUE)
else()
  set(QT_FOUND TRUE)
endif()

# DualC
include(FetchContent)

set(DUALC_GIT_TAG "2fcb6ee")  # Pinned commit from main after #52, io-stress-windows merged
if(DEFINED DUALCEDITOR_DUALC_DIR)
  add_subdirectory(${DUALCEDITOR_DUALC_DIR} ${CMAKE_BINARY_DIR}/DualC)
else()
  FetchContent_Declare(DualC
    GIT_REPOSITORY https://github.com/Giacogiak/DualC.git
    GIT_TAG ${DUALC_GIT_TAG}
  )
  FetchContent_MakeAvailable(DualC)
endif()

# Main executable
set(SOURCES
  src/main.cpp
  src/viewport.cpp
  src/viewport.h
)

if(Qt5_FOUND)
  add_executable(DualCEditor ${SOURCES})
  target_link_libraries(DualCEditor
    PRIVATE
    Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Widgets
    dualc::dualc
  )
else()
  add_executable(DualCEditor ${SOURCES})
  target_link_libraries(DualCEditor
    PRIVATE
    Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Widgets
    dualc::dualc
  )
endif()

# Warnings
if(DUALCEDITOR_WERROR)
  if(MSVC)
    target_compile_options(DualCEditor PRIVATE /WX)
  else()
    target_compile_options(DualCEditor PRIVATE -Werror)
  endif()
endif()

# Tests
if(DUALCEDITOR_BUILD_TESTS)
  enable_testing()
  # Add test targets here
endif()
