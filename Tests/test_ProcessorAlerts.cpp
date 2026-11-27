#include "test_ProcessorFixture.h"

#include "CtrlrInlineUtilitiesGUI.h"
#include "CtrlrApplicationWindow/CtrlrEditor.h"
#include "CtrlrPanel/CtrlrPanelEditor.h"
#include "CtrlrLua/MethodEditor/CtrlrLuaMethodEditor.h"
#include "CtrlrLua/MethodEditor/CtrlrLuaMethodCodeEditor.h"
#include "CtrlrLua/MethodEditor/CtrlrLuaMethodEditArea.h"
#include "CtrlrLuaManager.h"
#include "CtrlrWindowManagers/CtrlrPanelWindowManager.h"

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

/**
 * Closing a panel, the Lua editor and the app: the questions are asked as before (the Lua
 * editor's unsaved code first, then the panel's changes), and the close waits for the answers,
 * also for the Save chooser of a panel that was never saved.
 */
class CloseQuestions : public ProcessorInstance
{
protected:
    juce::File dir;
    juce::StringArray askedTitles;   // alerts and choosers, in order
    juce::StringArray buttonsToPress; // one per alert, in order; a missing one = the last button
    juce::Array<juce::File> chooserAnswer;

    void SetUp() override
    {
        ProcessorInstance::SetUp();
        allowAndRecordDeviceSends(); // the panels open the mock devices; MIDI is not the subject
        const auto *test = ::testing::UnitTest::GetInstance()->current_test_info();
        dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile (juce::String ("ctrlrx_aw_") + test->name() + "_" + juce::Uuid().toString());
        ASSERT_TRUE(dir.createDirectory().wasOk());

        AW::testAnswer() = [this] (const AW::Request &request) {
            askedTitles.add (request.title);
            const juce::String button = buttonsToPress.isEmpty() ? juce::String() : buttonsToPress[0];
            buttonsToPress.remove (0);
            int index = request.buttons.indexOf (button);
            if (index < 0)
                index = request.buttons.size() - 1;
            return AW::resultForButton (index, request.buttons.size());
        };
        FC::testAnswer() = [this] (const juce::String &title, int) {
            askedTitles.add (title);
            return chooserAnswer;
        };
    }

    void TearDown() override
    {
        AW::testAnswer() = nullptr;
        FC::testAnswer() = nullptr;
        dir.deleteRecursively();
        ProcessorInstance::TearDown();
    }

    static void pump() { juce::MessageManager::getInstance()->runDispatchLoopUntil (500); }

    static bool pumpUntil (std::function<bool()> done)
    {
        for (int i = 0; i < 100 && ! done(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        return done();
    }

    // A loaded fixture with changes and no file: a Save asks for the file.
    CtrlrPanel *changedUnsavedPanel()
    {
        CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
        if (panel == nullptr)
            return nullptr;
        panel->setProperty (Ids::panelFilePath, "");
        panel->setProperty (Ids::panelLastSaveDir, dir.getFullPathName());
        panel->setPanelDirty (true);
        return panel;
    }

    static void changeFirstMethod (CtrlrLuaMethodEditor &editor, CtrlrPanel &panel)
    {
        CtrlrLuaMethod *method = panel.getCtrlrLuaManager().getMethodManager().getMethodByName ("onTargetChange");
        ASSERT_NE(method, nullptr);
        editor.createNewTab (method);
        CtrlrLuaMethodCodeEditor *code = editor.getCurrentEditor();
        ASSERT_NE(code, nullptr);
        code->getCodeDocument().insertText (0, "-- changed\n");
    }
};

TEST_F(CloseQuestions, save_waits_for_the_chooser_and_writes_the_file_before_the_close)
{
    CtrlrPanel *panel = changedUnsavedPanel();
    ASSERT_NE(panel, nullptr);
    const juce::File target = dir.getChildFile ("saved.panel");
    chooserAnswer = { target };
    buttonsToPress = { "Save" };

    int calls = 0;
    bool got = false;
    bool fileWhenAnswered = false;
    panel->canClose (true, [&] (bool close) {
        ++calls;
        got = close;
        fileWhenAnswered = target.existsAsFile();
    });
    EXPECT_EQ(calls, 0) << "nothing is decided inside the call";

    ASSERT_TRUE(pumpUntil ([&] { return calls > 0; }));
    EXPECT_EQ(askedTitles, juce::StringArray ("Save panel (" + panel->getName() + ")", "Panel file XML"));
    EXPECT_TRUE(got);
    EXPECT_TRUE(fileWhenAnswered) << "the file is written before the panel may close";
    EXPECT_EQ(panel->getProperty (Ids::panelFilePath).toString(), target.getFullPathName());
}

// As on 5.6.37: a Save whose chooser was cancelled still lets the panel close (the changes are
// lost). Left for the fix PR.
TEST_F(CloseQuestions, save_with_the_chooser_cancelled_still_closes_as_before)
{
    CtrlrPanel *panel = changedUnsavedPanel();
    ASSERT_NE(panel, nullptr);
    chooserAnswer = {}; // Cancel
    buttonsToPress = { "Save" };

    int calls = 0;
    bool got = false;
    panel->canClose (true, [&] (bool close) {
        ++calls;
        got = close;
    });
    ASSERT_TRUE(pumpUntil ([&] { return calls > 0; }));
    EXPECT_EQ(askedTitles.size(), 2) << "the question and the chooser";
    EXPECT_TRUE(got);
    EXPECT_EQ(dir.getNumberOfChildFiles (juce::File::findFiles), 0);
}

TEST_F(CloseQuestions, discard_closes_without_a_chooser)
{
    CtrlrPanel *panel = changedUnsavedPanel();
    ASSERT_NE(panel, nullptr);
    buttonsToPress = { "Discard" };

    int calls = 0;
    bool got = false;
    panel->canClose (true, [&] (bool close) {
        ++calls;
        got = close;
    });
    ASSERT_TRUE(pumpUntil ([&] { return calls > 0; }));
    EXPECT_EQ(askedTitles, juce::StringArray ("Save panel (" + panel->getName() + ")"));
    EXPECT_TRUE(got);
}

TEST_F(CloseQuestions, cancel_keeps_the_panel)
{
    CtrlrPanel *panel = changedUnsavedPanel();
    ASSERT_NE(panel, nullptr);
    buttonsToPress = { "Cancel" };

    int calls = 0;
    bool got = true;
    panel->canClose (true, [&] (bool close) {
        ++calls;
        got = close;
    });
    ASSERT_TRUE(pumpUntil ([&] { return calls > 0; }));
    EXPECT_EQ(askedTitles.size(), 1);
    EXPECT_FALSE(got);
}

TEST_F(CloseQuestions, an_unchanged_panel_closes_without_a_question)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    panel->setPanelDirty (false);
    panel->setSavePoint();

    int calls = 0;
    bool got = false;
    panel->canClose (true, [&] (bool close) {
        ++calls;
        got = close;
    });
    pump();
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(got);
    EXPECT_TRUE(askedTitles.isEmpty());
}

// The panel is deleted (by Lua, or the host) while "Save panel?" is open: no chooser, no close.
TEST_F(CloseQuestions, panel_deleted_before_the_answer_is_left_alone)
{
    CtrlrPanel *panel = changedUnsavedPanel();
    ASSERT_NE(panel, nullptr);
    chooserAnswer = { dir.getChildFile ("saved.panel") };
    buttonsToPress = { "Save" }; // its result reaches canClose one message later

    int calls = 0;
    bool got = true;
    panel->canClose (true, [&] (bool close) {
        ++calls;
        got = close;
    });
    processor->getManager().removePanel (panel->getEditor (false));
    ASSERT_EQ(processor->getManager().getNumPanels(), 0);

    pump();
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(got);
    EXPECT_EQ(askedTitles.size(), 1) << "no chooser";
    EXPECT_EQ(dir.getNumberOfChildFiles (juce::File::findFiles), 0);
}

// The Lua editor's question: Save saves every method and closes, Cancel keeps it open.
TEST_F(CloseQuestions, lua_editor_asks_about_unsaved_code)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);

    for (const juce::String button : { "Cancel", "Discard", "Save" })
    {
        askedTitles.clear();
        CtrlrLuaMethodEditor editor (*panel);
        changeFirstMethod (editor, *panel);
        buttonsToPress = { button };

        int calls = 0;
        bool got = button == "Cancel";
        editor.canCloseWindow ([&] (bool close) {
            ++calls;
            got = close;
        });
        EXPECT_EQ(calls, 0) << button;
        ASSERT_TRUE(pumpUntil ([&] { return calls > 0; })) << button;
        EXPECT_EQ(askedTitles, juce::StringArray ("Save changes (" + panel->getName() + ")")) << button;
        EXPECT_EQ(got, button != "Cancel") << button;
        if (button == "Save")
            EXPECT_FALSE(editor.getCurrentEditor()->getCodeDocument().hasChangedSinceSavePoint());
    }
}

TEST_F(CloseQuestions, lua_editor_without_changes_closes_at_once)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    CtrlrLuaMethodEditor editor (*panel);

    int calls = 0;
    editor.canCloseWindow ([&] (bool close) {
        ++calls;
        EXPECT_TRUE(close);
    });
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(askedTitles.isEmpty());
}

// The order of 5.6.37: the Lua editor first, then the panel. Answering the panel's question with
// Save or Discard closes the panel even after Cancel in the Lua editor (as before).
TEST_F(CloseQuestions, panel_close_asks_the_lua_editor_first)
{
    CtrlrPanel *panel = changedUnsavedPanel();
    ASSERT_NE(panel, nullptr);
    // getContent() creates the editor's window without showing it (no window on a desktop).
    auto *editor = dynamic_cast<CtrlrLuaMethodEditor *> (panel->getWindowManager().getContent (CtrlrPanelWindowManager::LuaMethodEditor));
    ASSERT_NE(editor, nullptr);
    changeFirstMethod (*editor, *panel);
    buttonsToPress = { "Cancel", "Discard" };

    int calls = 0;
    bool got = false;
    panel->canClose (true, [&] (bool close) {
        ++calls;
        got = close;
    });
    ASSERT_TRUE(pumpUntil ([&] { return calls > 0; }));
    EXPECT_EQ(askedTitles, juce::StringArray ("Save changes (" + panel->getName() + ")", "Save panel (" + panel->getName() + ")"));
    EXPECT_TRUE(got);
}

// Quitting asks each panel's Lua editor in turn (not the panels' own changes, as before) and
// stops at the first Cancel.
TEST_F(CloseQuestions, quit_asks_each_lua_editor_in_turn_and_stops_at_cancel)
{
    CtrlrPanel *first = changedUnsavedPanel();
    ASSERT_NE(first, nullptr);
    CtrlrPanel *second = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(second, nullptr);
    ASSERT_EQ(processor->getManager().getNumPanels(), 2);
    for (CtrlrPanel *p : { first, second })
    {
        auto *editor = dynamic_cast<CtrlrLuaMethodEditor *> (p->getWindowManager().getContent (CtrlrPanelWindowManager::LuaMethodEditor));
        ASSERT_NE(editor, nullptr);
        changeFirstMethod (*editor, *p);
    }

    buttonsToPress = { "Discard", "Cancel" };
    int calls = 0;
    bool got = true;
    processor->getManager().canCloseWindow ([&] (bool close) {
        ++calls;
        got = close;
    });
    ASSERT_TRUE(pumpUntil ([&] { return calls > 0; }));
    EXPECT_EQ(askedTitles.size(), 2) << "one question per Lua editor, none about the panels";
    EXPECT_FALSE(got);

    askedTitles.clear();
    buttonsToPress = { "Discard", "Discard" };
    calls = 0;
    processor->getManager().canCloseWindow ([&] (bool close) {
        ++calls;
        got = close;
    });
    ASSERT_TRUE(pumpUntil ([&] { return calls > 0; }));
    EXPECT_EQ(askedTitles.size(), 2);
    EXPECT_TRUE(got);
}

TEST_F(CloseQuestions, quit_without_lua_changes_closes_at_once)
{
    ASSERT_NE(changedUnsavedPanel(), nullptr);
    int calls = 0;
    bool got = false;
    processor->getManager().canCloseWindow ([&] (bool close) {
        ++calls;
        got = close;
    });
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(got);
    EXPECT_TRUE(askedTitles.isEmpty()) << "quitting doesn't ask about the panel's changes, as before";
}
