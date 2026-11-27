#include "test_ProcessorFixture.h"

#include "CtrlrInlineUtilitiesGUI.h"

/**
 * The asynchronous alert helpers in CtrlrInlineUtilitiesGUI.h (namespace AW), through their test
 * hook: no alert is shown, the hook presses a button. Each helper must show the alert of the
 * blocking AlertWindow call it replaces (title, message, buttons in order), call back only later
 * from the message loop (never inside the call), and give the result the blocking call returned.
 */
class AlertHelpers : public ::testing::Test
{
protected:
    int hookCalls = 0;
    int answer = 0;
    AW::Request asked;

    void SetUp() override
    {
        AW::testAnswer() = [this] (const AW::Request &request) {
            ++hookCalls;
            asked = request;
            return answer;
        };
    }

    void TearDown() override { AW::testAnswer() = nullptr; }

    static void pump() { juce::MessageManager::getInstance()->runDispatchLoopUntil (50); }
};

TEST_F(AlertHelpers, result_for_button_is_the_blocking_calls_result)
{
    EXPECT_EQ(AW::resultForButton (0, 1), 0); // OK of a message box
    EXPECT_EQ(AW::resultForButton (0, 2), 1); // OK / Cancel
    EXPECT_EQ(AW::resultForButton (1, 2), 0);
    EXPECT_EQ(AW::resultForButton (0, 3), 1); // Yes / No / Cancel
    EXPECT_EQ(AW::resultForButton (1, 3), 2);
    EXPECT_EQ(AW::resultForButton (2, 3), 0);
}

TEST_F(AlertHelpers, message_box_shows_its_text_and_calls_back_later)
{
    int calls = 0;
    AW::showMessageBox (juce::MessageBoxIconType::WarningIcon, "Title", "Message", "", nullptr, [&] { ++calls; });

    EXPECT_EQ(hookCalls, 1);
    EXPECT_EQ(asked.title, "Title");
    EXPECT_EQ(asked.message, "Message");
    EXPECT_EQ(asked.buttons, juce::StringArray ("OK")) << "an empty button text is OK, as in the blocking call";
    EXPECT_EQ(calls, 0) << "the callback must not run inside the call";

    pump();
    EXPECT_EQ(calls, 1);
}

TEST_F(AlertHelpers, message_box_without_a_callback)
{
    AW::showMessageBox (juce::MessageBoxIconType::InfoIcon, "Title", "Message");
    EXPECT_EQ(hookCalls, 1);
    pump();
}

TEST_F(AlertHelpers, ok_cancel_gives_true_for_the_first_button)
{
    for (const int index : { 0, 1 })
    {
        answer = AW::resultForButton (index, 2);
        int calls = 0;
        bool got = index == 0 ? false : true;
        AW::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Sure?", "Really?", "Yes", "No", nullptr, [&] (bool ok) {
            ++calls;
            got = ok;
        });
        EXPECT_EQ(asked.buttons, juce::StringArray ("Yes", "No"));
        EXPECT_EQ(calls, 0);
        pump();
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(got, index == 0);
    }
}

TEST_F(AlertHelpers, yes_no_cancel_gives_1_2_0)
{
    for (const int index : { 0, 1, 2 })
    {
        answer = AW::resultForButton (index, 3);
        int got = -1;
        AW::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon, "Save?", "Changes", "Save", "Discard", "Cancel",
                                nullptr, [&] (int result) { got = result; });
        EXPECT_EQ(asked.buttons, juce::StringArray ("Save", "Discard", "Cancel"));
        EXPECT_EQ(got, -1);
        pump();
        EXPECT_EQ(got, index == 0 ? 1 : index == 1 ? 2 : 0);
    }
}

// One alert at a time, as the blocking calls showed them: the second opens after the first has
// closed and its callback has run (an error, then the summary of the same action).
TEST_F(AlertHelpers, alerts_open_one_after_the_other_in_order)
{
    juce::StringArray events;
    AW::testAnswer() = [&] (const AW::Request &request) {
        events.add ("open " + request.title);
        return 0;
    };
    AW::showMessageBox (juce::MessageBoxIconType::WarningIcon, "first", "", "", nullptr, [&] { events.add ("closed first"); });
    AW::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "second", "", "Yes", "No", nullptr,
                         [&] (bool) { events.add ("closed second"); });
    EXPECT_EQ(events, juce::StringArray ("open first")) << "the second waits";

    pump();
    EXPECT_EQ(events, juce::StringArray ("open first", "closed first", "open second", "closed second"));

    events.clear();
    AW::showMessageBox (juce::MessageBoxIconType::InfoIcon, "third", "");
    EXPECT_EQ(events, juce::StringArray ("open third")) << "an empty queue opens at once";
    pump();
}

// runModalAsync: the hook gets the window (to fill its fields), the callback gets the result and
// the window, later; the window is deleted after the callback.
TEST_F(AlertHelpers, run_modal_async_gives_the_window_to_the_callback_then_deletes_it)
{
    struct Window : juce::AlertWindow
    {
        Window (bool &deleted) : juce::AlertWindow ("Name", "Enter a name", juce::MessageBoxIconType::NoIcon), deletedFlag (deleted)
        {
            addTextEditor ("name", "");
            addButton ("OK", 1);
            addButton ("Cancel", 0);
        }
        ~Window() override { deletedFlag = true; }
        bool &deletedFlag;
    };

    bool deleted = false;
    AW::testAnswer() = [this] (const AW::Request &request) {
        ++hookCalls;
        asked = request;
        if (auto *w = dynamic_cast<juce::AlertWindow *> (request.window))
            w->getTextEditor ("name")->setText ("typed");
        return 1;
    };

    int calls = 0;
    int got = -1;
    juce::String text;
    AW::runModalAsync<juce::AlertWindow> (std::make_unique<Window> (deleted), [&] (int result, juce::AlertWindow &w) {
        ++calls;
        got = result;
        text = w.getTextEditorContents ("name");
        EXPECT_FALSE(deleted) << "the window still exists in the callback";
        EXPECT_FALSE(w.isVisible()) << "but it is hidden";
    });
    EXPECT_EQ(hookCalls, 1);
    EXPECT_EQ(asked.title, "Name");
    EXPECT_EQ(calls, 0) << "the callback must not run inside the call";
    EXPECT_FALSE(deleted);

    pump();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(got, 1);
    EXPECT_EQ(text, "typed");
    EXPECT_TRUE(deleted);
}
