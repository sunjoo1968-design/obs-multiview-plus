cmake_minimum_required(VERSION 3.28)
project(obs-multiview-plus VERSION 0.5.4 LANGUAGES CXX)

if(WIN32)
  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Windows x64 is required")
  endif()
elseif(NOT APPLE)
  message(FATAL_ERROR "Windows x64 or macOS is required")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOGEN_PARALLEL 1)

if(WIN32)
  set(OBS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-studio-32.2.2" CACHE PATH "OBS 32.2.2 source headers")
  set(OBS_IMPORT_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-imports" CACHE PATH "OBS import libraries")
  set(MV_QT_MIN_VERSION 6.11)
  set(MV_WARNINGS /W4 /utf-8)
else()
  # macOS: headers come from the OBS source tag matching the installed OBS;
  # the libraries come from the installed OBS.app itself (see scripts/build-macos.sh).
  set(OBS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-studio" CACHE PATH "OBS source tree (headers only) matching the installed OBS")
  set(OBS_APP "/Applications/OBS.app" CACHE PATH "Installed OBS.app that provides libobs and obs-frontend-api")
  # libobs headers use simde (SIMD compatibility layer) on every non-MSVC build.
  set(OBS_SIMDE_DIR "${CMAKE_SOURCE_DIR}/.deps/simde" CACHE PATH "simde source tree (contains the simde/ header folder)")
  if(NOT EXISTS "${OBS_SIMDE_DIR}/simde/x86/sse2.h")
    message(FATAL_ERROR "simde headers not found in ${OBS_SIMDE_DIR}. Run scripts/build-macos.sh or pass -DOBS_SIMDE_DIR=...")
  endif()
  set(MV_QT_MIN_VERSION 6)
  set(MV_WARNINGS -Wall -Wextra)
endif()
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/.deps/qt6")
find_package(Qt6 ${MV_QT_MIN_VERSION} REQUIRED COMPONENTS Widgets)

set(OBS_RELEASE_CANDIDATE 0)
set(OBS_BETA 0)
configure_file("${OBS_SOURCE_DIR}/libobs/obsconfig.h.in" "${CMAKE_BINARY_DIR}/generated/obsconfig.h")
add_library(obs-sdk INTERFACE)
target_compile_definitions(obs-sdk INTERFACE MV_VERSION="${PROJECT_VERSION}")
target_include_directories(obs-sdk SYSTEM INTERFACE
  "${OBS_SOURCE_DIR}/libobs" "${OBS_SOURCE_DIR}/frontend/api" "${CMAKE_BINARY_DIR}/generated")

if(WIN32)
  target_link_libraries(obs-sdk INTERFACE "${OBS_IMPORT_DIR}/obs.lib" "${OBS_IMPORT_DIR}/obs-frontend-api.lib")
else()
  target_include_directories(obs-sdk SYSTEM INTERFACE "${OBS_SIMDE_DIR}")
  list(LENGTH CMAKE_OSX_ARCHITECTURES MV_ARCH_COUNT)
  if(MV_ARCH_COUNT GREATER 1)
    # Universal build: the installed OBS.app contains only one architecture, so its
    # libraries cannot be linked for both. Resolve libobs symbols when OBS loads the plugin.
    message(STATUS "Universal build (${CMAKE_OSX_ARCHITECTURES}): libobs symbols resolve at load time")
    target_link_options(obs-sdk INTERFACE -Wl,-undefined,dynamic_lookup)
  else()
    set(MV_LIBOBS "")
    foreach(candidate
        "${OBS_APP}/Contents/Frameworks/libobs.framework/libobs"
        "${OBS_APP}/Contents/Frameworks/libobs.framework/Versions/A/libobs")
      if(NOT MV_LIBOBS AND EXISTS "${candidate}")
        set(MV_LIBOBS "${candidate}")
      endif()
    endforeach()
    file(GLOB MV_FRONTEND_CANDIDATES
      "${OBS_APP}/Contents/Frameworks/obs-frontend-api.dylib"
      "${OBS_APP}/Contents/Frameworks/libobs-frontend-api.dylib"
      "${OBS_APP}/Contents/Frameworks/obs-frontend-api*.dylib"
      "${OBS_APP}/Contents/Frameworks/libobs-frontend-api*.dylib")
    set(MV_FRONTEND_API "")
    if(MV_FRONTEND_CANDIDATES)
      list(GET MV_FRONTEND_CANDIDATES 0 MV_FRONTEND_API)
    endif()
    if(NOT MV_LIBOBS OR NOT MV_FRONTEND_API)
      message(FATAL_ERROR "libobs or obs-frontend-api not found in ${OBS_APP}/Contents/Frameworks. "
                          "Install OBS or pass -DOBS_APP=/path/to/OBS.app")
    endif()
    message(STATUS "Linking libobs: ${MV_LIBOBS}")
    message(STATUS "Linking frontend API: ${MV_FRONTEND_API}")
    target_link_libraries(obs-sdk INTERFACE "${MV_LIBOBS}" "${MV_FRONTEND_API}")
  endif()
endif()

option(MV_BUILD_SMOKE_DRIVER "Build isolated OBS runtime test driver (never installed)" OFF)
if(MV_BUILD_SMOKE_DRIVER AND NOT WIN32)
  message(WARNING "MV_BUILD_SMOKE_DRIVER is only supported on Windows and is ignored here")
endif()
if(MV_BUILD_SMOKE_DRIVER AND WIN32)
  add_library(obs-multiview-leak-probe MODULE tests/leak-probe.cpp)
  target_link_libraries(obs-multiview-leak-probe PRIVATE obs-sdk Qt6::Widgets)
  target_compile_options(obs-multiview-leak-probe PRIVATE ${MV_WARNINGS})
  set_target_properties(obs-multiview-leak-probe PROPERTIES PREFIX "")
  add_library(obs-multiview-smoke MODULE tests/obs-smoke-driver.cpp)
  target_include_directories(obs-multiview-smoke PRIVATE src)
  target_link_libraries(obs-multiview-smoke PRIVATE obs-sdk Qt6::Widgets)
  target_compile_options(obs-multiview-smoke PRIVATE ${MV_WARNINGS})
  target_compile_definitions(obs-multiview-smoke PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  set_target_properties(obs-multiview-smoke PROPERTIES PREFIX "")
  add_library(obs-multiview-hybrid-probe MODULE tests/hybrid-probe.cpp src/layout-model.cpp)
  target_include_directories(obs-multiview-hybrid-probe PRIVATE src)
  target_link_libraries(obs-multiview-hybrid-probe PRIVATE obs-sdk Qt6::Widgets)
  target_compile_options(obs-multiview-hybrid-probe PRIVATE ${MV_WARNINGS})
  set_target_properties(obs-multiview-hybrid-probe PROPERTIES PREFIX "")

endif()

add_library(obs-multiview-plus MODULE
  src/plugin-main.cpp src/layout-model.cpp src/layout-model.hpp
  src/layout-preview.cpp src/layout-preview.hpp
  src/settings-dialog.cpp src/settings-dialog.hpp src/video-tile.cpp src/video-tile.hpp
  src/resource-monitor.cpp src/resource-monitor.hpp)
target_link_libraries(obs-multiview-plus PRIVATE obs-sdk Qt6::Widgets)
if(WIN32)
  target_link_libraries(obs-multiview-plus PRIVATE pdh)
  target_compile_definitions(obs-multiview-plus PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(obs-multiview-plus PRIVATE ${MV_WARNINGS} /permissive-)
else()
  target_compile_options(obs-multiview-plus PRIVATE ${MV_WARNINGS})
  # Qt must come from the running OBS.app, never from the build machine's .deps folder:
  # a second Qt copy inside one process crashes. Use only the app-relative search path.
  set_target_properties(obs-multiview-plus PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path/../Frameworks")
endif()
set_target_properties(obs-multiview-plus PROPERTIES PREFIX "")
install(TARGETS obs-multiview-plus RUNTIME DESTINATION obs-plugins/64bit LIBRARY DESTINATION obs-plugins/64bit)
install(FILES README.md LICENSE DESTINATION data/obs-plugins/obs-multiview-plus)

include(CTest)
if(BUILD_TESTING)
  add_executable(layout-test tests/layout-test.cpp src/layout-model.cpp)
  target_include_directories(layout-test PRIVATE src)
  target_link_libraries(layout-test PRIVATE Qt6::Widgets)
  target_compile_options(layout-test PRIVATE ${MV_WARNINGS})
  add_test(NAME layout-model COMMAND layout-test)
  if(WIN32)
    set_tests_properties(layout-model PROPERTIES ENVIRONMENT "PATH=${CMAKE_SOURCE_DIR}/.deps/qt6/bin\;$ENV{PATH};QT_QPA_PLATFORM=offscreen")
  else()
    # Qt from obs-deps keeps its platform plugins next to lib/; point the test there.
    set_tests_properties(layout-model PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${Qt6_DIR}/../../../plugins")
  endif()
  add_executable(tally-test tests/tally-test.cpp)
  target_include_directories(tally-test PRIVATE src)
  target_compile_options(tally-test PRIVATE ${MV_WARNINGS})
  add_test(NAME tally-graph COMMAND tally-test)
endif()
