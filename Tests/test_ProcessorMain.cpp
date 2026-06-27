// Custom test entry point.
//
// Replaces the stock gtest_main/gmock_main so we can bracket the whole test run with a
// juce::ScopedJuceInitialiser_GUI. Without it, the JUCE GUI singletons created while panels are
// loaded (Desktop, the default LookAndFeel, ComponentAnimator, ...) are never torn down, which
// produces the "*** Leaked objects detected" reports and the juce_Singleton.h assertions at exit.
// The initialiser's destructor runs shutdownJuce_GUI() after RUN_ALL_TESTS() returns, cleaning
// those singletons up.

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "JuceHeader.h"

int main(int argc, char** argv)
{
    // InitGoogleMock also initialises GoogleTest and parses the gtest/gmock command-line flags.
    ::testing::InitGoogleMock(&argc, argv);

    juce::ScopedJuceInitialiser_GUI juceGui;

    return RUN_ALL_TESTS();
}
