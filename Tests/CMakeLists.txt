include(FetchContent)
FetchContent_Declare(
    googletest
    # Specify the commit you depend on and update it regularly.
    # Note that we use C++14, so we should only choose a commit that is not depending on higher c++ standards.
    URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip # C++14
    # URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip # C++17
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

message(STATUS "Tests enabled")

# Copy the test panel to the build directory
configure_file("fixture_host2host_cc.panel" "fixture_host2host_cc.panel" COPYONLY)
configure_file("fixture_host2device_cc.panel" "fixture_host2device_cc.panel" COPYONLY)
configure_file("fixture_device_input_cc.panel" "fixture_device_input_cc.panel" COPYONLY)
configure_file("fixture_nrpn_stream.panel" "fixture_nrpn_stream.panel" COPYONLY)
configure_file("fixture_sysex_token.panel" "fixture_sysex_token.panel" COPYONLY)
configure_file("fixture_program_change.panel" "fixture_program_change.panel" COPYONLY)
configure_file("fixture_lua_midi_received.panel" "fixture_lua_midi_received.panel" COPYONLY)
configure_file("fixture_lua_multi_received.panel" "fixture_lua_multi_received.panel" COPYONLY)
configure_file("fixture_lua_send.panel" "fixture_lua_send.panel" COPYONLY)
configure_file("fixture_lua_nrpn.panel" "fixture_lua_nrpn.panel" COPYONLY)

# find all test sources
file(GLOB_RECURSE MockFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/mock*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/mock*.h")
file(GLOB_RECURSE TestProcessorFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_Processor*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/test_Processor*.h")
# Organize the test source in the Tests/ folder in Xcode
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/ PREFIX "" FILES ${MockFiles} ${TestProcessorFiles})

add_executable(TestsProcessor ${MockFiles} ${TestProcessorFiles})
target_link_libraries(TestsProcessor
    PRIVATE
    gtest_main
    gmock_main
    ${PROJECT_NAME})
target_compile_definitions(TestsProcessor PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME},COMPILE_DEFINITIONS>)
target_include_directories(TestsProcessor PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME},INCLUDE_DIRECTORIES>)

# Change 14 to 17 if you use JUCE 8
target_compile_features(TestsProcessor PRIVATE cxx_std_14)

if ((DEFINED ENV{CI}))
    target_compile_definitions(TestsProcessor PUBLIC CI=1)
endif ()

add_test(NAME TestsProcessor COMMAND TestsProcessor)

