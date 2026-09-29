cmake_minimum_required(VERSION 3.29)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(WIN32)
        project(BuffPanel VERSION 1.0.11 LANGUAGES CXX RC)
    else()
        project(BuffPanel VERSION 1.0.11 LANGUAGES CXX)
    endif()
elseif(WIN32)
    enable_language(RC)
endif()

if(NOT TARGET D2RLPlugin::D2RLPlugin AND NOT TARGET D2RLPluginV4::D2RLPluginV4)
    foreach(BUFF_PANEL_SDK IN ITEMS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/PluginSDK"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/PluginSDK-V4"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/PluginSDK-v4"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/PluginSDK")
        if(EXISTS "${BUFF_PANEL_SDK}/CMakeLists.txt"
                AND EXISTS "${BUFF_PANEL_SDK}/include/D2RLPlugin/version.h")
            file(READ "${BUFF_PANEL_SDK}/include/D2RLPlugin/version.h" SDK_VERSION_HEADER)
            if(SDK_VERSION_HEADER MATCHES "D2RL_PLUGIN_ABI_VERSION[ \t]+4([^0-9]|$)"
                    AND SDK_VERSION_HEADER MATCHES "D2RL_SDK_VERSION[ \t]+\"0[.]2[.][0-9]+\"")
                add_subdirectory("${BUFF_PANEL_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/PluginSDK")
                break()
            endif()
        endif()
    endforeach()
endif()
if(NOT TARGET D2RLPlugin::D2RLPlugin AND NOT TARGET D2RLPluginV4::D2RLPluginV4)
    find_package(D2RLPlugin 0.2.0 CONFIG QUIET)
endif()
if(TARGET D2RLPlugin::D2RLPlugin)
    set(BUFF_PANEL_SDK_TARGET D2RLPlugin::D2RLPlugin)
elseif(TARGET D2RLPluginV4::D2RLPluginV4)
    set(BUFF_PANEL_SDK_TARGET D2RLPluginV4::D2RLPluginV4)
else()
    message(FATAL_ERROR "Buff Panel requires official D2RLoader PluginSDK 0.2.x (plugin ABI 4). Update Suite third_party/PluginSDK.")
endif()

# 1.0.6+ keeps the complete defaults embedded in the compiled C++ sources.
# There are intentionally no configure-time TXT/JSON/template dependencies so
# source-mirroring installers cannot lose assets required to build the DLL.
add_library(buff_panel SHARED
    src/plugin.cpp
    src/core/services.cpp
    src/core/hook_registry.cpp
    src/core/stat_list_post_bus.cpp
    src/core/stat_read_bus.cpp
    src/core/buff_display_bus.cpp
    src/systems/buff_tracker/buff_tracker.cpp
    src/systems/buff_tracker/loose_buff_hud.cpp
    src/systems/buff_hud/buff_hud.cpp
    src/systems/buff_hud/loose_layout.cpp
    src/systems/buff_hud/icon_frame_backend.cpp
    src/systems/buff_hud/skill_icon_resolver.cpp
)
if(WIN32)
    target_sources(buff_panel PRIVATE src/plugin.rc)
endif()
target_include_directories(buff_panel PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(buff_panel PRIVATE ${BUFF_PANEL_SDK_TARGET})
target_compile_features(buff_panel PRIVATE cxx_std_20)
if(WIN32)
    target_compile_definitions(buff_panel PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
set_target_properties(buff_panel PROPERTIES OUTPUT_NAME "d2rl-buff-panel")
if(MSVC)
    target_compile_options(buff_panel PRIVATE /W4 /permissive- /utf-8)
endif()


option(BUFF_PANEL_BUILD_TESTS "Build Buff Panel regression tests" OFF)
if(BUFF_PANEL_BUILD_TESTS)
    enable_testing()

    add_executable(buff_panel_timer_metadata_test tests/timer_metadata_test.cpp)
    target_include_directories(buff_panel_timer_metadata_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(buff_panel_timer_metadata_test PRIVATE cxx_std_20)
    add_test(NAME buff_panel_timer_metadata COMMAND buff_panel_timer_metadata_test)

    add_executable(buff_panel_tooltip_text_test tests/tooltip_text_test.cpp)
    target_include_directories(buff_panel_tooltip_text_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(buff_panel_tooltip_text_test PRIVATE cxx_std_20)
    add_test(NAME buff_panel_tooltip_text COMMAND buff_panel_tooltip_text_test)

    add_executable(buff_panel_skill_icon_resolver_test
        tests/skill_icon_resolver_test.cpp
        src/systems/buff_hud/skill_icon_resolver.cpp)
    target_include_directories(buff_panel_skill_icon_resolver_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(buff_panel_skill_icon_resolver_test PRIVATE ${BUFF_PANEL_SDK_TARGET})
    target_compile_features(buff_panel_skill_icon_resolver_test PRIVATE cxx_std_20)
    if(WIN32)
        target_compile_definitions(buff_panel_skill_icon_resolver_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    add_test(NAME buff_panel_skill_icon_resolver COMMAND buff_panel_skill_icon_resolver_test)

    # This test includes production buff_hud.cpp directly so it is deliberately
    # Windows-only and built only when the explicit test option is enabled.
    if(WIN32)
        add_executable(buff_panel_tooltip_binding_test
            tests/tooltip_binding_test.cpp
            src/core/services.cpp
            src/core/buff_display_bus.cpp
            src/systems/buff_hud/loose_layout.cpp
            src/systems/buff_hud/skill_icon_resolver.cpp
            src/systems/buff_hud/icon_frame_backend.cpp)
        target_include_directories(buff_panel_tooltip_binding_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(buff_panel_tooltip_binding_test PRIVATE ${BUFF_PANEL_SDK_TARGET})
        target_compile_definitions(buff_panel_tooltip_binding_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_features(buff_panel_tooltip_binding_test PRIVATE cxx_std_20)
        add_test(NAME buff_panel_tooltip_binding COMMAND buff_panel_tooltip_binding_test)
    endif()
endif()
