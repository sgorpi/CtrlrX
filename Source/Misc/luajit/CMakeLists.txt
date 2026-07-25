cmake_minimum_required(VERSION 3.10)
project(LuaJIT C)

set(LUAJIT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(WIN32)
    # Windows: use pre-built static lib from msvcbuild.bat
    # IMPORTANT: must be built with x64 Native Tools Command Prompt — see LUAJIT_BUILD.md
    add_library(libluajit STATIC IMPORTED GLOBAL)
    set_target_properties(libluajit PROPERTIES
        IMPORTED_LOCATION         "${LUAJIT_SRC}/lua51.lib"
        IMPORTED_LOCATION_DEBUG   "${LUAJIT_SRC}/lua51.lib"
        IMPORTED_LOCATION_RELEASE "${LUAJIT_SRC}/lua51.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_SRC}"
    )
elseif(APPLE)
    # macOS: use locally built universal binary (arm64 + x86_64)
    # Built by the CI workflow Build Universal LuaJIT step
    add_library(libluajit STATIC IMPORTED GLOBAL)
    set_target_properties(libluajit PROPERTIES
        IMPORTED_LOCATION             "${LUAJIT_SRC}/libluajit.a"
        IMPORTED_LOCATION_DEBUG       "${LUAJIT_SRC}/libluajit.a"
        IMPORTED_LOCATION_RELEASE     "${LUAJIT_SRC}/libluajit.a"
        INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_SRC}"
    )
else()
    # Linux: use system LuaJIT via pkg-config
    # Install with: sudo apt install libluajit-5.1-dev
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LUAJIT REQUIRED luajit)
    add_library(libluajit INTERFACE)
    target_include_directories(libluajit INTERFACE ${LUAJIT_INCLUDE_DIRS})
    target_link_libraries(libluajit INTERFACE ${LUAJIT_LIBRARIES})
    target_link_directories(libluajit INTERFACE ${LUAJIT_LIBRARY_DIRS})
endif()
