cmake_minimum_required(VERSION 3.28)
project(obs-multiview-plus VERSION 0.5.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Windows x64 is required")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOGEN_PARALLEL 1)
set(OBS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-studio-32.2.2" CACHE PATH "OBS 32.2.2 source headers")
set(OBS_IMPORT_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-imports" CACHE PATH "OBS import libraries")
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/.deps/qt6")
find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets)

set(OBS_RELEASE_CANDIDATE 0)
set(OBS_BETA 0)
configure_file("${OBS_SOURCE_DIR}/libobs/obsconfig.h.in" "${CMAKE_BINARY_DIR}/generated/obsconfig.h")
add_library(obs-sdk INTERFACE)
target_include_directories(obs-sdk SYSTEM INTERFACE
  "${OBS_SOURCE_DIR}/libobs" "${OBS_SOURCE_DIR}/frontend/api" "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(obs-sdk INTERFACE "${OBS_IMPORT_DIR}/obs.lib" "${OBS_IMPORT_DIR}/obs-frontend-api.lib")

option(MV_BUILD_SMOKE_DRIVER "Build isolated OBS runtime test driver (never installed)" OFF)
if(MV_BUILD_SMOKE_DRIVER)
  add_library(obs-multiview-smoke MODULE tests/obs-smoke-driver.cpp)
  target_include_directories(obs-multiview-smoke PRIVATE src)
  target_link_libraries(obs-multiview-smoke PRIVATE obs-sdk Qt6::Widgets)
  target_compile_options(obs-multiview-smoke PRIVATE /W4 /utf-8)
  target_compile_definitions(obs-multiview-smoke PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  set_target_properties(obs-multiview-smoke PROPERTIES PREFIX "")
endif()

add_library(obs-multiview-plus MODULE
  src/plugin-main.cpp src/layout-model.cpp src/layout-model.hpp
  src/layout-preview.cpp src/layout-preview.hpp
  src/settings-dialog.cpp src/settings-dialog.hpp src/video-tile.cpp src/video-tile.hpp
  src/resource-monitor.cpp src/resource-monitor.hpp)
target_link_libraries(obs-multiview-plus PRIVATE obs-sdk Qt6::Widgets pdh)
target_compile_definitions(obs-multiview-plus PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(obs-multiview-plus PRIVATE /W4 /utf-8 /permissive-)
set_target_properties(obs-multiview-plus PROPERTIES PREFIX "")
install(TARGETS obs-multiview-plus RUNTIME DESTINATION obs-plugins/64bit LIBRARY DESTINATION obs-plugins/64bit)
install(FILES README.md LICENSE DESTINATION data/obs-plugins/obs-multiview-plus)

include(CTest)
if(BUILD_TESTING)
  add_executable(layout-test tests/layout-test.cpp src/layout-model.cpp)
  target_include_directories(layout-test PRIVATE src)
  target_link_libraries(layout-test PRIVATE Qt6::Widgets)
  target_compile_options(layout-test PRIVATE /W4 /utf-8)
  add_test(NAME layout-model COMMAND layout-test)
  set_tests_properties(layout-model PROPERTIES ENVIRONMENT "PATH=${CMAKE_SOURCE_DIR}/.deps/qt6/bin\;$ENV{PATH};QT_QPA_PLATFORM=offscreen")
  add_executable(tally-test tests/tally-test.cpp)
  target_include_directories(tally-test PRIVATE src)
  target_compile_options(tally-test PRIVATE /W4 /utf-8)
  add_test(NAME tally-graph COMMAND tally-test)
endif()
