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


#if JUCE_LINUX
// Added v8.0.100. Create the X atoms that JUCE looks up for its windows, as the panel probe does
// (prepareX11Display() in probe_Main.cpp, which explains it in full). JUCE asks for them with
// XInternAtom(only_if_exists = True); on a bare X server with no window manager (xvfb-run, CI)
// they don't exist, the first property change on a new desktop window fails with BadAtom, and
// Xlib exits the process. A test that opens a window (an alert, a child window) then passed only
// when another test process had created the atoms first. On a desktop they already exist. The
// connection stays open until the process exits: the X server deletes the atoms again when its
// last client disconnects. Keep the list the same as the probe's.
void prepareX11Display()
{
    using XOpenDisplayFn = void* (*) (const char*);
    using XInternAtomsFn = int (*) (void*, char**, int, int, unsigned long*);
    static juce::DynamicLibrary xLib;
    if (! xLib.open("libX11.so.6"))
        return;
    auto xOpenDisplay = (XOpenDisplayFn) xLib.getFunction("XOpenDisplay");
    auto xInternAtoms = (XInternAtomsFn) xLib.getFunction("XInternAtoms");
    if (xOpenDisplay == nullptr || xInternAtoms == nullptr)
        return;

    // The display JUCE opens: $DISPLAY, else ":0.0".
    const char* name = std::getenv("DISPLAY");
    static void* display = nullptr;
    display = xOpenDisplay(name != nullptr && *name != 0 ? name : ":0.0");
    if (display == nullptr)
        return;

    static const char* const names[] = {
        // XWindowSystemUtilities::Atoms
        "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "_NET_WM_PING", "WM_CHANGE_STATE",
        "WM_STATE", "_NET_WM_WINDOW_TYPE", "_NET_WM_STATE", "_NET_WM_STATE_HIDDEN",
        // XWindowSystem
        "_NET_WM_STATE_FULLSCREEN", "_NET_FRAME_EXTENTS", "_NET_WORKAREA", "_MOTIF_WM_HINTS",
        "_WIN_HINTS", "KWM_WIN_DECORATION", "_KDE_NET_WM_WINDOW_TYPE_OVERRIDE",
        "_NET_WM_ALLOWED_ACTIONS", "_NET_WM_WINDOW_TYPE_COMBO", "_NET_WM_WINDOW_TYPE_NORMAL",
        "_NET_WM_ACTION_RESIZE", "_NET_WM_ACTION_FULLSCREEN", "_NET_WM_ACTION_MINIMIZE",
        "_NET_WM_ACTION_CLOSE", "_NET_WM_STATE_SKIP_TASKBAR",
        "_NET_WM_MOVERESIZE", // JUCE 8 only
    };
    constexpr int numNames = (int) (sizeof(names) / sizeof(names[0]));
    unsigned long atoms[numNames] = {};
    // only_if_exists = False: create the missing ones. A round trip, so they exist on return.
    xInternAtoms(display, const_cast<char**>(names), numNames, 0, atoms);
}
#endif

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

   #if JUCE_LINUX
    prepareX11Display(); // before JUCE connects to the X server
   #endif
    juce::ScopedJuceInitialiser_GUI juceGui;

    return RUN_ALL_TESTS();
}
