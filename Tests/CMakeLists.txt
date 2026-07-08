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

# Provides gtest_discover_tests(), which registers every TEST_F/TEST_P as its own ctest
# case so `ctest -j` can run them in parallel, each in its own process (true isolation).
include(GoogleTest)

message(STATUS "Tests enabled")

# Copy the test fixture panels to the build directory
file(GLOB FixtureFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/*.panel")
foreach(FixtureFile ${FixtureFiles})
    get_filename_component(FixtureName "${FixtureFile}" NAME)
    configure_file("${FixtureFile}" "fixtures/${FixtureName}" COPYONLY)
endforeach()

# find all test sources
file(GLOB_RECURSE MockFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/mock*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/mock*.h")
file(GLOB_RECURSE TestProcessorFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_Processor*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/test_Processor*.h")
# Organize the test source in the Tests/ folder in Xcode
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/ PREFIX "" FILES ${MockFiles} ${TestProcessorFiles})

add_executable(TestsProcessor ${MockFiles} ${TestProcessorFiles})
# NOTE: link gtest/gmock (NOT gtest_main/gmock_main). TestsProcessor provides its own main()
# in test_ProcessorMain.cpp so it can bracket the run with a juce::ScopedJuceInitialiser_GUI.
target_link_libraries(TestsProcessor
    PRIVATE
    gtest
    gmock
    ${PROJECT_NAME})
target_compile_definitions(TestsProcessor PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME},COMPILE_DEFINITIONS>)
target_include_directories(TestsProcessor PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME},INCLUDE_DIRECTORIES>)

# Change 14 to 17 if you use JUCE 8
target_compile_features(TestsProcessor PRIVATE cxx_std_14)

if ((DEFINED ENV{CI}))
    target_compile_definitions(TestsProcessor PUBLIC CI=1)
endif ()

# Register each GoogleTest case (including every parameterised TEST_P/.../0,/1 instance) as an
# individual ctest test, named with its exact gtest fully-qualified name -- the same strings used
# in known_failures.txt. PRE_TEST enumerates at ctest time rather than running the freshly-built
# binary during `cmake --build`. Run with `ctest -jN` for parallel, per-process isolation.
gtest_discover_tests(TestsProcessor
    DISCOVERY_MODE PRE_TEST
    PROPERTIES TIMEOUT 120)

