cmake_minimum_required(VERSION 3.25)

# Suppress CMake 3.31+ policy warnings
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.31.0") 
    cmake_policy(SET CMP0175 NEW)
endif()

# allows clang-based tools to work with the source:
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PamplejuceVersion)

# -----------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------
option(CTRLRX_ENABLE_CPM "Enable CPM-based optional dependencies" OFF)
option(CTRLRX_USE_SYSTEM_JUCE "Use system-installed JUCE instead of vendored JUCE submodule" OFF)
option(CTRLRX_COPY_PLUGIN_AFTER_BUILD "Copy plugin artefacts to user/system plugin folders after build" OFF)
option(CTRLRX_USE_LUAJIT "Use LuaJIT instead of standard Lua" ON)

if(CTRLRX_ENABLE_CPM)
    include(CPM)
endif()

include(PamplejuceMacOS)
include(JUCEDefaults)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# -----------------------------------------------------------------------
# Project identity
# -----------------------------------------------------------------------
set(BASE_ID_NAME "CtrlrX")
set(PROJECT_NAME "${BASE_ID_NAME}")
set(CMAKE_CXX_STANDARD 20) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_DISPLAY_NAME "CtrlrX")
set(PRODUCT_NAME "CtrlrX")
set(COMPANY_NAME "CtrlrX Project")
set(BUNDLE_ID "com.${BASE_ID_NAME}.${PROJECT_NAME}")
set(FORMATS Standalone AU VST3)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

# -----------------------------------------------------------------------
# Linux System Packages
# -----------------------------------------------------------------------
if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libudev REQUIRED libudev)
    pkg_check_modules(x11 REQUIRED x11)
    pkg_check_modules(zlib REQUIRED zlib)
    pkg_check_modules(gtk3 REQUIRED gtk+-3.0) # Fixed target name
    pkg_check_modules(webkit REQUIRED webkit2gtk-4.1)

    include_directories(SYSTEM
        ${gtk3_INCLUDE_DIRS}
        ${webkit_INCLUDE_DIRS}
        ${x11_INCLUDE_DIRS}
    )
    set(JUCE_USE_WEBKIT_4_1 ON CACHE INTERNAL "")
endif()

# -----------------------------------------------------------------------
# Subdirectories (JUCE + Lua Engine)
# -----------------------------------------------------------------------
if(CTRLRX_USE_SYSTEM_JUCE)
    find_package(JUCE CONFIG REQUIRED)
else()
    add_subdirectory(JUCE)
endif()

if(CTRLRX_USE_LUAJIT)
    # Source/Misc/luajit/CMakeLists.txt builds the vendored LuaJIT source and
    # defines the `libluajit` target. It carries the LuaJIT include directories
    # (both the source tree and the generated headers) as INTERFACE include
    # directories, so linking it is enough.
    add_subdirectory(Source/Misc/luajit)
else()
    message(FATAL_ERROR
        "CTRLRX_USE_LUAJIT=OFF is not supported: the vendored Source/Misc/lua "
        "tree is incomplete, and Source/Misc/luajit/src is on the include path "
        "unconditionally further down this file. To build against a system "
        "LuaJIT instead of the vendored source, configure with "
        "-DCTRLRX_LUAJIT_USE_SYSTEM=ON.")
endif()

# -----------------------------------------------------------------------
# SDK Paths
# -----------------------------------------------------------------------
if("VST" IN_LIST FORMATS)
    juce_set_vst2_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/vst2sdk")
endif()

if("AAX" IN_LIST FORMATS)
    juce_set_aax_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/aax-sdk-2-8-1")
endif()

# -----------------------------------------------------------------------
# JUCE Plugin Target Configuration
# -----------------------------------------------------------------------
juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Source/Resources/Icons/ctrlrx_logo.svg"
    PLUGIN_NAME "${PLUGIN_DISPLAY_NAME}"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD ${CTRLRX_COPY_PLUGIN_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE cTrX
    PLUGIN_CODE cTrl
    FORMATS "${FORMATS}"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PRODUCT_NAME "${PRODUCT_NAME}"
)

# -----------------------------------------------------------------------
# SharedCode Library & Source Files
# -----------------------------------------------------------------------
add_library(SharedCode INTERFACE)
include(SharedCodeDefaults)

# Optimization fix for LuaBind
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/luabind/src/object_rep.cpp"
    PROPERTIES COMPILE_FLAGS "-O0"
)

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/luabind/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/luabind/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/luabind/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/luabind/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/libusb/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/libusb/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/*.c*"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/*.h*"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Lua/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Lua/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/MIDI/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/MIDI/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Native/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Native/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Plugin/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Plugin/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UIComponents/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UIComponents/*.h"
)

if(NOT LINUX)
    list(FILTER SourceFiles EXCLUDE REGEX "CtrlrLinux\\.(cpp|h)$")
endif()

# Exclude generated/foreign sources
list(FILTER SourceFiles EXCLUDE REGEX ".*/Source/Misc/luajit/.*")
list(FILTER SourceFiles EXCLUDE REGEX ".*/Source/Misc/lua/src/.*")
list(FILTER SourceFiles EXCLUDE REGEX ".*/Source/Misc/luabind/(examples|build[^/]*|test|tests)/.*")
list(FILTER SourceFiles EXCLUDE REGEX ".*/Source/Lua/Deprecated/CtrlrLuaFile\\.cpp$")

target_sources(SharedCode INTERFACE ${SourceFiles})

target_include_directories(SharedCode INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/luajit/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/luabind"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/libusb/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/boost"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Misc/vst2sdk"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/MIDI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/CtrlrManager"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/CtrlrModulator"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/CtrlrPanel"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Native"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Plugin"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UIComponents"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/JuceLibraryCode"
)

add_compile_definitions(BOOST_BIND_GLOBAL_PLACEHOLDERS)

target_compile_definitions(SharedCode INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    CTLRX_DISABLE_DYNAMIC_LNF=0
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
    $<$<PLATFORM_ID:Linux>:_GLIBCXX_USE_DEPRECATED=1>
)

# Missing Compile Options Fix
target_compile_options(SharedCode INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-msse2 -fPIC>
)

if(CTRLRX_USE_LUAJIT)
    target_compile_definitions(SharedCode INTERFACE CTRLRX_USE_LUAJIT=1)
endif()

# -----------------------------------------------------------------------
# ASSETS & BINARY DATA (The Important Part)
# -----------------------------------------------------------------------
include(Assets)
include(XcodePrettify)

# -----------------------------------------------------------------------
# Linking
# -----------------------------------------------------------------------
target_link_libraries(SharedCode INTERFACE
    Assets  
    juce_audio_basics
    juce_audio_devices
    juce_audio_formats
    juce_audio_plugin_client
    juce_audio_processors
    juce_audio_utils
    juce_core
    juce_cryptography
    juce_data_structures
    juce_events
    juce_graphics
    juce_gui_basics
    juce_gui_extra
    juce_product_unlocking
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(CTRLRX_USE_LUAJIT)
    target_link_libraries(SharedCode INTERFACE libluajit)
endif()

# Linux specific library linking to resolve XSendEvent and graphics dependencies
if(LINUX)
    target_link_libraries(SharedCode INTERFACE
        ${x11_LIBRARIES}
        ${gtk3_LIBRARIES}
        ${webkit_LIBRARIES}
        ${libudev_LIBRARIES}
        ${zlib_LIBRARIES}
    )
endif()

# Final Plugin Target Linking
target_link_libraries("${PROJECT_NAME}" PRIVATE
    SharedCode
    Assets 
)

include(PamplejuceIPP)

# Everything related to the tests target
#include(Tests) # This would use the Catch2 framework and recompile the source-code...
enable_testing()
add_subdirectory(Tests)

# A separate target for Benchmarks (keeps the Tests target fast)
#include(Benchmarks) # enable once we have benchmarks...

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)
