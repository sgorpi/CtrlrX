// Custom test entry point.
//
// Replaces the stock gtest_main/gmock_main so we can bracket the whole test run with a
// juce::ScopedJuceInitialiser_GUI. Without it, the JUCE GUI singletons created while panels are
// loaded (Desktop, the default LookAndFeel, ComponentAnimator, ...) are never torn down, which
// produces the "*** Leaked objects detected" reports and the juce_Singleton.h assertions at exit.
// The initialiser's destructor runs shutdownJuce_GUI() after RUN_ALL_TESTS() returns, cleaning
// those singletons up.

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "JuceHeader.h"

namespace {

std::string sanitizeForFilename(std::string s)
{
    for (char& c : s)
        if (!std::isalnum((unsigned char) c) && c != '.' && c != '_' && c != '-')
            c = '_';
    return s;
}

// gtest_discover_tests + `ctest -j` runs this binary once per test, each in its own process with a
// single `--gtest_filter`. When the requested XML output is a *directory* (path ends in '/'),
// derive a per-test filename from the filter so those parallel invocations each write their own
// file instead of racing on one shared file -- gtest reuses the executable's base name for the
// directory form, so every process would otherwise clobber (or race on) the same TestsProcessor.xml.
//
// Returns the full `--gtest_output=xml:<dir>/<test>.xml` argument to inject, or "" to leave output
// as-is. This MUST run before InitGoogleMock: gtest wires up the XML result printer during flag
// parsing, so the output path has to be settled by then. It reads the effective filter/output from
// argv (falling back to the GTEST_FILTER / GTEST_OUTPUT env vars, which is how CI passes output),
// and returns a command-line token so it overrides the env value regardless of platform.
std::string computePerTestXmlOutputArg(int argc, char** argv)
{
    std::string filter, output;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        // gtest_discover_tests (PRE_TEST) enumerates by re-running us with --gtest_list_tests; with
        // GTEST_OUTPUT set that would dump a full 41-test result skeleton. Suppress it so only real
        // per-test runs produce XML.
        if (a == "--gtest_list_tests")
            return "--gtest_output=";
        if (a.rfind("--gtest_filter=", 0) == 0)
            filter = a.substr(std::string("--gtest_filter=").size());
        else if (a.rfind("--gtest_output=", 0) == 0)
            output = a.substr(std::string("--gtest_output=").size());
    }
    if (filter.empty())
        if (const char* e = std::getenv("GTEST_FILTER"))
            filter = e;
    if (output.empty())
        if (const char* e = std::getenv("GTEST_OUTPUT"))
            output = e;

    const bool isDirOutput = output.rfind("xml:", 0) == 0 && output.back() == '/';
    const bool isSingleTest = !filter.empty()
        && filter.find('*') == std::string::npos
        && filter.find(':') == std::string::npos
        && filter.find('-') == std::string::npos;
    if (!isDirOutput || !isSingleTest)
        return {};

    return "--gtest_output=" + output + sanitizeForFilename(filter) + ".xml";
}

} // namespace

int main(int argc, char** argv)
{
    // Give each parallel `ctest` invocation its own XML file (see helper). Must happen before
    // InitGoogleMock; a command-line --gtest_output overrides the CI-provided GTEST_OUTPUT env var.
    const std::string perTestOutput = computePerTestXmlOutputArg(argc, argv);

    std::vector<char*> args(argv, argv + argc);
    if (!perTestOutput.empty())
    {
        static std::string outputArg;
        outputArg = perTestOutput; // must outlive InitGoogleMock, which stores pointers into it
        bool replaced = false;
        for (char*& a : args)
            if (std::string(a).rfind("--gtest_output=", 0) == 0)
            {
                a = &outputArg[0];
                replaced = true;
                break;
            }
        if (!replaced)
            args.push_back(&outputArg[0]);
    }
    int newArgc = static_cast<int>(args.size());
    args.push_back(nullptr); // argv must be null-terminated

    // InitGoogleMock also initialises GoogleTest and parses the gtest/gmock command-line flags.
    ::testing::InitGoogleMock(&newArgc, args.data());

    juce::ScopedJuceInitialiser_GUI juceGui;

    return RUN_ALL_TESTS();
}
