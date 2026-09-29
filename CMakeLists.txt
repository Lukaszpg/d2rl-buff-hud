cmake_minimum_required(VERSION 3.29)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BuffPanel VERSION 1.0.11 LANGUAGES CXX RC)
elseif(WIN32)
    enable_language(RC)
endif()

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Buff Panel requires Windows x64 and an MSVC-compatible toolchain.")
endif()

set(BUFF_PANEL_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/PluginSDK" CACHE PATH "PluginSDK source directory (0.3.0 or newer, ABI 4)")
if(NOT TARGET D2RLPlugin::D2RLPlugin AND NOT TARGET D2RLPluginV4::D2RLPluginV4)
    if(EXISTS "${BUFF_PANEL_SDK_DIR}/CMakeLists.txt")
        file(STRINGS "${BUFF_PANEL_SDK_DIR}/include/D2RLPlugin/version.h" sdk_version REGEX "^#define[ \t]+D2RL_SDK_VERSION")
        if(NOT sdk_version MATCHES "\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" OR CMAKE_MATCH_1 VERSION_LESS 0.3.0)
            message(FATAL_ERROR "Buff Panel requires PluginSDK 0.3.0 or newer.")
        endif()
        set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        add_subdirectory("${BUFF_PANEL_SDK_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/PluginSDK" EXCLUDE_FROM_ALL)
    else()
        find_package(D2RLPlugin 0.3.0 CONFIG REQUIRED)
    endif()
endif()

if(TARGET D2RLPlugin::D2RLPlugin)
    set(BUFF_PANEL_SDK_TARGET D2RLPlugin::D2RLPlugin)
elseif(TARGET D2RLPluginV4::D2RLPluginV4)
    set(BUFF_PANEL_SDK_TARGET D2RLPluginV4::D2RLPluginV4)
else()
    message(FATAL_ERROR "Buff Panel requires PluginSDK 0.3.0 or a Suite-provided ABI-4 PluginSDK target.")
endif()

add_library(buff_panel SHARED
    src/plugin.cpp
    src/plugin.rc
    src/core/services.cpp
    src/core/hook_registry.cpp
    src/core/stat_list_post_bus.cpp
    src/core/stat_read_bus.cpp
    src/core/buff_display_bus.cpp
    src/systems/buff_tracker/buff_tracker.cpp
    src/systems/buff_hud/buff_hud.cpp
    src/systems/buff_hud/icon_frame_backend.cpp
    src/systems/buff_hud/skill_icon_resolver.cpp
)
target_include_directories(buff_panel PRIVATE src)
target_link_libraries(buff_panel PRIVATE ${BUFF_PANEL_SDK_TARGET})
target_compile_features(buff_panel PRIVATE cxx_std_20)
target_compile_definitions(buff_panel PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(buff_panel PRIVATE /W4 /permissive- /utf-8 /EHsc)
set_target_properties(buff_panel PROPERTIES
    OUTPUT_NAME "d2rl-buff-panel"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage/$<CONFIG>/d2rloader/plugins"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdb/$<CONFIG>"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Validate the actual shipping assets, not a second set embedded in C++.
add_executable(buff_panel_validate
    tests/validate_resources.cpp
    src/systems/buff_hud/loose_layout.cpp
    src/systems/buff_tracker/loose_buff_hud.cpp)
target_include_directories(buff_panel_validate PRIVATE src)
target_compile_features(buff_panel_validate PRIVATE cxx_std_20)
target_compile_options(buff_panel_validate PRIVATE /utf-8 /EHsc)

set(BUFF_PANEL_SDK_LICENSE_FILE "${BUFF_PANEL_SDK_DIR}/LICENSE" CACHE FILEPATH "PluginSDK license to include in dist")
set(D2RL_COMPILER "${CMAKE_CURRENT_SOURCE_DIR}/../d2rl-compiler/D2RLCompiler.exe" CACHE FILEPATH "D2RLCompiler.exe used to pack the companion MPQ")
set(BUFF_PANEL_LAYOUT "${CMAKE_CURRENT_SOURCE_DIR}/companion/data/global/ui/layouts/buff-panel/BuffHudhd.json")
set(BUFF_PANEL_TABLE "${CMAKE_CURRENT_SOURCE_DIR}/companion/data/global/excel/d2rloader/buff-panel/buff-hud.txt")
# Run on every build so an asset-only edit (or deletion) cannot leave a stale MPQ.
# Custom-table TXT is compiled by D2RLoader after registerTable; pack needs no game link.
add_custom_target(buff_panel_companion
    COMMAND buff_panel_validate "${BUFF_PANEL_LAYOUT}" "${BUFF_PANEL_TABLE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:buff_panel>"
    COMMAND "${CMAKE_COMMAND}"
        "-DD2RL_COMPILER=${D2RL_COMPILER}"
        "-DBUFF_PANEL_COMPANION_DIR=${CMAKE_CURRENT_SOURCE_DIR}/companion"
        "-DBUFF_PANEL_COMPANION_OUTPUT=$<TARGET_FILE_DIR:buff_panel>/d2rl-buff-panel.mpq"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackCompanion.cmake"
    DEPENDS buff_panel_validate
    COMMENT "Validating and packing Buff Panel companion resources"
    VERBATIM)
add_custom_target(dist
    COMMAND "${CMAKE_COMMAND}"
        "-DBUFF_PANEL_CONFIG=$<CONFIG>"
        "-DBUFF_PANEL_SDK_LICENSE_FILE=${BUFF_PANEL_SDK_LICENSE_FILE}"
        "-DBUFF_PANEL_PLUGIN_DIR=$<TARGET_FILE_DIR:buff_panel>"
        "-DBUFF_PANEL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DBUFF_PANEL_DIST_DIR=${CMAKE_CURRENT_SOURCE_DIR}/dist"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PublishDist.cmake"
    DEPENDS buff_panel_companion buff_panel
    COMMENT "Publishing the Buff Panel DLL and MPQ pair"
    VERBATIM)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(BUFF_PANEL_BUILD_TESTS "Build Buff Panel regression tests" ON)
else()
    option(BUFF_PANEL_BUILD_TESTS "Build Buff Panel regression tests" OFF)
endif()
if(BUFF_PANEL_BUILD_TESTS)
    enable_testing()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME production_surface_source
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/production_surface_source_test.py")
    endif()
    add_executable(timer_metadata_test tests/timer_metadata_test.cpp)
    target_include_directories(timer_metadata_test PRIVATE src)
    target_compile_features(timer_metadata_test PRIVATE cxx_std_20)
    add_test(NAME timer_metadata COMMAND timer_metadata_test)
    add_executable(tooltip_text_test tests/tooltip_text_test.cpp)
    target_include_directories(tooltip_text_test PRIVATE src)
    target_compile_features(tooltip_text_test PRIVATE cxx_std_20)
    add_test(NAME tooltip_text COMMAND tooltip_text_test)
    add_executable(tooltip_binding_test tests/tooltip_binding_test.cpp
        src/core/services.cpp src/core/buff_display_bus.cpp
        src/systems/buff_hud/skill_icon_resolver.cpp src/systems/buff_hud/icon_frame_backend.cpp)
    target_include_directories(tooltip_binding_test PRIVATE src)
    target_link_libraries(tooltip_binding_test PRIVATE D2RLPlugin::D2RLPlugin)
    target_compile_definitions(tooltip_binding_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(tooltip_binding_test PRIVATE /utf-8 /EHsc)
    add_test(NAME tooltip_binding COMMAND tooltip_binding_test)
    add_executable(skill_icon_resolver_test tests/skill_icon_resolver_test.cpp src/systems/buff_hud/skill_icon_resolver.cpp)
    target_include_directories(skill_icon_resolver_test PRIVATE src)
    target_link_libraries(skill_icon_resolver_test PRIVATE D2RLPlugin::D2RLPlugin)
    target_compile_definitions(skill_icon_resolver_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(skill_icon_resolver_test PRIVATE /utf-8 /EHsc)
    add_test(NAME skill_icon_resolver COMMAND skill_icon_resolver_test)
    foreach(kind IN ITEMS layout buff_hud)
        add_executable(loose_${kind}_test tests/loose_${kind}_test.cpp)
        if(kind STREQUAL "layout")
            target_sources(loose_${kind}_test PRIVATE src/systems/buff_hud/loose_layout.cpp)
            add_test(NAME loose_${kind} COMMAND loose_${kind}_test "${BUFF_PANEL_LAYOUT}")
        else()
            target_sources(loose_${kind}_test PRIVATE src/systems/buff_tracker/loose_buff_hud.cpp)
            add_test(NAME loose_${kind} COMMAND loose_${kind}_test)
        endif()
        target_include_directories(loose_${kind}_test PRIVATE src)
        target_compile_features(loose_${kind}_test PRIVATE cxx_std_20)
        target_compile_options(loose_${kind}_test PRIVATE /utf-8 /EHsc)
    endforeach()
    add_test(NAME companion_assets COMMAND buff_panel_validate "${BUFF_PANEL_LAYOUT}" "${BUFF_PANEL_TABLE}")
endif()
