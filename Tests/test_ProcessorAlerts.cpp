#include "test_ProcessorFixture.h"

#include "CtrlrInlineUtilitiesGUI.h"
#include "CtrlrApplicationWindow/CtrlrEditor.h"
#include "CtrlrPanel/CtrlrPanelEditor.h"
#include "CtrlrLua/MethodEditor/CtrlrLuaMethodEditor.h"
#include "CtrlrLua/MethodEditor/CtrlrLuaMethodCodeEditor.h"
#include "CtrlrLua/MethodEditor/CtrlrLuaMethodEditArea.h"
#include "CtrlrLuaManager.h"
#include "CtrlrWindowManagers/CtrlrPanelWindowManager.h"
#include "CtrlrPanel/CtrlrPanelResourceEditor.h"
#include "CtrlrPanel/CtrlrPanelResourceManager.h"
#include "CtrlrLua/MethodEditor/CtrlrLuaMethodEditorTabs.h"
#include "Methods/CtrlrLuaMethodManager.h"
#include "CtrlrPropertyEditors/CtrlrPropertyComponent.h"
#include "CtrlrWindowManagers/CtrlrDialogWindow.h"
#include "CtrlrPanel/CtrlrPanelCanvas.h"
#include "CtrlrValueTreeEditor.h"
#include "CtrlrLua/MethodEditor/CtrlrLuaMethodCodeEditorSettings.h"

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

    // The tests that make a window: on a slow runner (macOS CI) the window's first-time work can
    // keep the message loop busy for more than 50 ms before the callback's message comes.
    static bool pumpUntil (std::function<bool()> done)
    {
        for (int i = 0; i < 100 && ! done(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        return done();
    }
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

    EXPECT_TRUE(pumpUntil ([&] { return calls == 1 && deleted; }));
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
        // Close the panels while the processor still exists: some tests end with a Lua editor
        // window open, and deleting the processor deletes its log before the panels, so the
        // editor would remove its log listener from the deleted log (a known shutdown bug, on
        // 5.6.37 too; a crash on CI's Release build).
        while (processor != nullptr && processor->getManager().getNumPanels() > 0)
        {
            CtrlrPanelEditor *editor = processor->getManager().getPanel (0)->getEditor (false);
            if (editor == nullptr)
                break;
            processor->getManager().removePanel (editor);
        }
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

/**
 * The C++ callers of the alert helpers: the alert opens through AW (after the dialog before it
 * has closed), and the code that waited for an answer runs in the callback.
 */
class AlertCallers : public CloseQuestions
{
};

// Export with resources, chooser cancelled: exportPanel's message in a warning, after the chooser.
TEST_F(AlertCallers, export_with_resources_cancelled_warns_after_the_chooser)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    chooserAnswer = {}; // Cancel

    panel->savePanelAs (CtrlrEditor::doExportFileZBinRes);
    EXPECT_EQ(askedTitles, juce::StringArray ("Panel file binary compressed"));

    ASSERT_TRUE(pumpUntil ([&] { return askedTitles.size() == 2; }));
    EXPECT_EQ(askedTitles[1], "Panel Export");
}

// Resource editor, "Add": a file that is already a resource asks "Overwrite?". No stops there (the
// files after it are not added), Yes replaces it and goes on, as the blocking loop did.
TEST_F(AlertCallers, resource_add_asks_before_overwriting_and_no_stops)
{
    // Resources are copied into the panel's folder in the CtrlrX data directory, named by its UID.
    // A UID of this test's own, so no other panel (or parallel test) sees these files.
    const juce::String uid = "test-" + juce::Uuid().toString();
    juce::ValueTree tree = CtrlrPanel::openXmlPanel (juce::File::getCurrentWorkingDirectory().getChildFile ("fixtures/fixture_lua_values.panel"));
    ASSERT_TRUE(tree.isValid());
    tree.setProperty (Ids::panelUID, uid, nullptr);
    CtrlrPanel *panel = processor->getManager().addPanel (tree, true);
    ASSERT_NE(panel, nullptr);
    ASSERT_EQ(panel->getProperty (Ids::panelUID).toString(), uid);
    struct RemoveFolder
    {
        juce::File folder;
        ~RemoveFolder() { folder.deleteRecursively(); }
    } removeFolder { panel->getPanelDirectory() };
    ASSERT_TRUE(removeFolder.folder.getFileName() == uid);
    processor->getManager().setProperty (Ids::ctrlrOverwriteResources, false);
    const juce::File existing = dir.getChildFile ("existing.txt");
    const juce::File added = dir.getChildFile ("added.txt");
    ASSERT_TRUE(existing.replaceWithText ("one"));
    ASSERT_TRUE(added.replaceWithText ("two"));
    CtrlrPanelResourceManager &resources = panel->getResourceManager();
    ASSERT_TRUE(resources.addResource (existing).wasOk());
    auto editor = std::make_unique<CtrlrPanelResourceEditor> (*panel->getEditor (false));

    for (const juce::String button : { "No", "Yes" })
    {
        askedTitles.clear();
        chooserAnswer = { existing, added };
        buttonsToPress = { button };
        editor->addResourceFromFile();
        ASSERT_TRUE(pumpUntil ([&] { return askedTitles.size() == 2; })) << button;
        EXPECT_EQ(askedTitles[1], "Are you sure?");
        pump();
        EXPECT_EQ(resources.resourceExists (added), button == "Yes") << button;
    }
}

TEST_F(AlertCallers, resource_move_waits_for_the_answer_and_skips_a_closed_editor)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    auto editor = std::make_unique<CtrlrPanelResourceEditor> (*panel->getEditor (false));
    buttonsToPress = { "Yes" };

    editor->moveResources();
    EXPECT_EQ(askedTitles, juce::StringArray ("Move resources to panel folder"));
    editor.reset();
    pump(); // the answer reaches a deleted editor: nothing happens (memcheck)
}

// The Lua method editor's dialogs: the hook fills the dialog's text box and presses OK.
static juce::StringArray namesIn (const juce::ValueTree &tree, const juce::Identifier &type, const juce::Identifier &nameId)
{
    juce::StringArray names;
    for (const auto &child : tree)
        if (child.hasType (type))
            names.add (child.getProperty (nameId).toString());
    return names;
}

class LuaEditorDialogs : public AlertCallers
{
protected:
    juce::String typed;  // put into the dialog's first text box
    int dialogResult = 1; // the dialog's button (1 = OK)

    void SetUp() override
    {
        AlertCallers::SetUp();
        AW::TestAnswer alerts = AW::testAnswer();
        AW::testAnswer() = [this, alerts] (const AW::Request &request) {
            if (request.window == nullptr)
                return alerts (request);
            askedTitles.add (request.title);
            if (auto *w = dynamic_cast<juce::AlertWindow *> (request.window))
                for (const juce::String name : { "methodName", "groupName", "name" })
                    if (auto *box = w->getTextEditor (name))
                        box->setText (typed);
            return dialogResult;
        };
    }

    juce::ValueTree root (CtrlrPanel &panel) { return panel.getCtrlrLuaManager().getMethodManager().getManagerTree(); }
};

TEST_F(LuaEditorDialogs, add_method_adds_it_in_the_callback)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    CtrlrLuaMethodEditor editor (*panel);
    typed = "addedByTest";

    editor.addNewMethod (root (*panel));
    EXPECT_EQ(askedTitles.size(), 1);
    EXPECT_FALSE(namesIn (root (*panel), Ids::luaMethod, Ids::luaMethodName).contains (typed)) << "nothing before the answer";
    EXPECT_TRUE(pumpUntil ([&] { return namesIn (root (*panel), Ids::luaMethod, Ids::luaMethodName).contains (typed); }));
}

TEST_F(LuaEditorDialogs, add_method_with_an_invalid_name_warns_after_the_dialog)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    CtrlrLuaMethodEditor editor (*panel);
    typed = "not a name";
    const int before = namesIn (root (*panel), Ids::luaMethod, Ids::luaMethodName).size();

    editor.addNewMethod (root (*panel));
    ASSERT_TRUE(pumpUntil ([&] { return askedTitles.size() == 2; }));
    EXPECT_EQ(askedTitles[1], "WARNING");
    EXPECT_EQ(namesIn (root (*panel), Ids::luaMethod, Ids::luaMethodName).size(), before);
}

TEST_F(LuaEditorDialogs, add_and_rename_group_in_the_callback)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    CtrlrLuaMethodEditor editor (*panel);

    typed = "Test group";
    editor.addNewGroup (root (*panel));
    EXPECT_FALSE(namesIn (root (*panel), Ids::luaMethodGroup, Ids::name).contains (typed));
    ASSERT_TRUE(pumpUntil ([&] { return namesIn (root (*panel), Ids::luaMethodGroup, Ids::name).contains (typed); }));

    juce::ValueTree group = root (*panel).getChildWithProperty (Ids::name, "Test group");
    typed = "Renamed group";
    editor.renameGroup (group);
    EXPECT_EQ(group.getProperty (Ids::name).toString(), "Test group");
    EXPECT_TRUE(pumpUntil ([&] { return group.getProperty (Ids::name).toString() == "Renamed group"; }));
}

TEST_F(LuaEditorDialogs, dialogs_do_nothing_when_the_editor_closed_before_the_answer)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    auto editor = std::make_unique<CtrlrLuaMethodEditor> (*panel);
    typed = "neverAdded";

    editor->addNewMethod (root (*panel));
    editor->addNewGroup (root (*panel));
    editor.reset();
    pump(); // no use of the deleted editor (memcheck)
    EXPECT_FALSE(namesIn (root (*panel), Ids::luaMethod, Ids::luaMethodName).contains (typed));
    EXPECT_FALSE(namesIn (root (*panel), Ids::luaMethodGroup, Ids::name).contains (typed));
}

TEST_F(LuaEditorDialogs, remove_group_with_methods_asks_first)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    CtrlrLuaMethodEditor editor (*panel);
    CtrlrLuaMethodManager &methods = panel->getCtrlrLuaManager().getMethodManager();
    methods.addGroup ("Full group");
    juce::ValueTree group = root (*panel).getChildWithProperty (Ids::name, "Full group");
    ASSERT_TRUE(group.isValid());
    methods.addMethod (group, "inFullGroup", "function inFullGroup()\nend", "");
    ASSERT_GT(group.getNumChildren(), 0);

    buttonsToPress = { "No" };
    editor.removeGroup (group);
    EXPECT_EQ(askedTitles, juce::StringArray ("Are you sure?"));
    pump();
    EXPECT_TRUE(namesIn (root (*panel), Ids::luaMethodGroup, Ids::name).contains ("Full group"));

    buttonsToPress = { "Yes" };
    editor.removeGroup (group);
    EXPECT_TRUE(pumpUntil ([&] { return ! namesIn (root (*panel), Ids::luaMethodGroup, Ids::name).contains ("Full group"); }));
}

// Closing tabs with unsaved code: each asks; No keeps the tab and stops "close all", as before.
TEST_F(LuaEditorDialogs, close_tabs_with_unsaved_code_ask_and_no_stops)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    CtrlrLuaMethodEditor editor (*panel);
    CtrlrLuaMethodManager &methods = panel->getCtrlrLuaManager().getMethodManager();
    for (const char *name : { "onTargetChange", "onOtherChange" })
    {
        CtrlrLuaMethod *method = methods.getMethodByName (name);
        ASSERT_NE(method, nullptr) << name;
        editor.createNewTab (method);
        editor.getCurrentEditor()->getCodeDocument().insertText (0, "-- changed\n");
    }
    CtrlrLuaMethodEditorTabs *tabs = editor.getMethodEditArea()->getTabs();
    ASSERT_EQ(tabs->getNumTabs(), 2);

    buttonsToPress = { "Yes", "No" };
    editor.closeAllTabs();
    EXPECT_EQ(tabs->getNumTabs(), 2) << "nothing closes before the answer";
    ASSERT_TRUE(pumpUntil ([&] { return askedTitles.size() == 2; }));
    pump();
    EXPECT_EQ(tabs->getNumTabs(), 1);
}

// R4a's method menu, then the question: the method goes only after Yes.
TEST_F(LuaEditorDialogs, remove_method_asks_after_the_menu)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    CtrlrLuaMethodEditor editor (*panel);
    juce::ValueTree method = root (*panel).getChildWithProperty (Ids::luaMethodName, "onOtherChange");
    ASSERT_TRUE(method.isValid());
    PU::testAnswer() = [] (const juce::PopupMenu &menu, const juce::PopupMenu::Options &) {
        for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
            if (it.getItem().text == "Remove method")
                return it.getItem().itemID;
        return 0;
    };
    const juce::Point<float> pos (5.0f, 5.0f);
    const juce::Time now = juce::Time::getCurrentTime();
    const juce::MouseEvent rightClick (juce::Desktop::getInstance().getMainMouseSource(), pos,
                                       juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier),
                                       juce::MouseInputSource::invalidPressure, juce::MouseInputSource::invalidOrientation,
                                       juce::MouseInputSource::invalidRotation, juce::MouseInputSource::invalidTiltX,
                                       juce::MouseInputSource::invalidTiltY, &editor, &editor, now, pos, now, 1, false);

    for (const juce::String button : { "No", "Yes" })
    {
        askedTitles.clear();
        buttonsToPress = { button };
        editor.itemClicked (rightClick, method);
        ASSERT_TRUE(pumpUntil ([&] { return askedTitles.size() == 1; })) << button;
        EXPECT_EQ(askedTitles[0], "Are you sure?");
        pump();
        EXPECT_EQ(namesIn (root (*panel), Ids::luaMethod, Ids::luaMethodName).contains ("onOtherChange"), button == "No") << button;
    }
    PU::testAnswer() = nullptr;
}

// CtrlrLog shows every logged error in a warning, from the message loop. Two errors: two
// warnings, one after the other.
TEST_F(AlertCallers, logged_errors_warn_one_after_the_other)
{
    _ERR("first test error");
    _ERR("second test error");
    EXPECT_TRUE(askedTitles.isEmpty()) << "the log shows them from the message loop";
    ASSERT_TRUE(pumpUntil ([&] { return askedTitles.size() == 2; }));
    EXPECT_EQ(askedTitles, juce::StringArray ("WARNING", "WARNING"));
}

// Property editors: an invalid expression warns (the field stays as typed), and the Lua method
// property's "New method" adds the method after its name dialog.
TEST_F(LuaEditorDialogs, expression_property_warns_about_an_invalid_expression)
{
    juce::Value value (juce::var ("1 +"));
    CtrlrExpressionProperty property (value);
    property.refresh();
    juce::TextEditor unused;

    property.textEditorReturnKeyPressed (unused);
    ASSERT_TRUE(pumpUntil ([&] { return askedTitles.size() == 1; }));
    EXPECT_EQ(askedTitles[0], "Expression validation");
}

TEST_F(LuaEditorDialogs, lua_method_property_adds_the_method_after_the_dialog)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    juce::Value value (juce::var (""));
    auto property = std::make_unique<CtrlrLuaMethodProperty> (value, Ids::luaModulatorValueChange, panel);
    juce::Button *add = nullptr;
    for (auto *child : property->getChildren())
        if (auto *b = dynamic_cast<juce::Button *> (child))
            if (b->getName() == "New Method")
                add = b;
    ASSERT_NE(add, nullptr);
    typed = "fromProperty";

    property->buttonClicked (add);
    EXPECT_EQ(askedTitles, juce::StringArray ("Method name"));
    EXPECT_TRUE(pumpUntil ([&] { return namesIn (root (*panel), Ids::luaMethod, Ids::luaMethodName).contains (typed); }));

    // Deleted before the answer: nothing is added, nothing is touched.
    typed = "neverAdded";
    property->buttonClicked (add);
    property.reset();
    pump();
    EXPECT_FALSE(namesIn (root (*panel), Ids::luaMethod, Ids::luaMethodName).contains ("neverAdded"));
}

/**
 * The licence of an exported panel. Export: after the chooser, "License question" and, for Yes,
 * the "License content" window; its text goes into the file. Load: "License agreement" is asked
 * before the panel is made, and the panel is made only for Yes.
 */
class LicenseQuestions : public CloseQuestions
{
protected:
    int windowResult = 0;       // what a dialog window (no buttons) answers
    juce::String licenseToType; // typed into the window's text box
    juce::String licenseShown;  // what its text box showed
    juce::File panelFolder;

    void SetUp() override
    {
        CloseQuestions::SetUp();
        AW::testAnswer() = [this, alerts = AW::testAnswer()] (const AW::Request &request) {
            if (request.window == nullptr)
                return alerts (request);
            askedTitles.add (request.title);
            if (auto *w = dynamic_cast<juce::AlertWindow *> (request.window))
                if (auto *text = dynamic_cast<juce::TextEditor *> (w->getCustomComponent (0)))
                {
                    licenseShown = text->getText();
                    if (licenseToType.isNotEmpty())
                        text->setText (licenseToType);
                }
            return windowResult;
        };
    }

    void TearDown() override
    {
        if (panelFolder != juce::File())
            panelFolder.deleteRecursively();
        CloseQuestions::TearDown();
    }

    // The fixture with one resource. Under a UID of this test's own: resources are copied into
    // the panel's folder in the CtrlrX data directory, named by its UID.
    CtrlrPanel *panelWithResource()
    {
        juce::ValueTree tree = CtrlrPanel::openXmlPanel (juce::File::getCurrentWorkingDirectory().getChildFile ("fixtures/fixture_lua_values.panel"));
        if (! tree.isValid())
            return nullptr;
        tree.setProperty (Ids::panelUID, "test-" + juce::Uuid().toString(), nullptr);
        CtrlrPanel *panel = processor->getManager().addPanel (tree, true);
        if (panel == nullptr)
            return nullptr;
        panelFolder = panel->getPanelDirectory();
        const juce::File resource = dir.getChildFile ("resource.txt");
        if (! resource.replaceWithText ("data") || ! panel->getResourceManager().addResource (resource).wasOk())
            return nullptr;
        return panel;
    }

    // The licence in an exported .bpanelz, or a void var if there is none.
    static juce::var licenseIn (const juce::File &exported)
    {
        juce::MemoryBlock data;
        if (! exported.loadFileAsData (data))
            return {};
        const juce::ValueTree tree = juce::ValueTree::readFromGZIPData (data.getData(), data.getSize());
        return tree.getChildWithName (Ids::resourceExportList).getChildWithName (Ids::resourceLicense).getProperty (Ids::resourceData);
    }

    // The fixture as an exported panel with a licence.
    static juce::ValueTree licensedTree()
    {
        juce::ValueTree tree = CtrlrPanel::openXmlPanel (juce::File::getCurrentWorkingDirectory().getChildFile ("fixtures/fixture_lua_values.panel"));
        juce::ValueTree resources (Ids::resourceExportList);
        juce::ValueTree licence (Ids::resourceLicense);
        licence.setProperty (Ids::resourceData, "the terms", nullptr);
        resources.addChild (licence, -1, nullptr);
        tree.addChild (resources, -1, nullptr);
        return tree;
    }
};

TEST_F(LicenseQuestions, export_asks_after_the_chooser_and_attaches_the_typed_license)
{
    CtrlrPanel *panel = panelWithResource();
    ASSERT_NE(panel, nullptr);
    const juce::File exported = dir.getChildFile ("out.bpanelz");
    chooserAnswer = { exported };
    buttonsToPress = { "Yes" };
    windowResult = 1; // its OK button
    licenseToType = "my licence";

    panel->savePanelAs (CtrlrEditor::doExportFileZBinRes);
    EXPECT_EQ(askedTitles, juce::StringArray ("Panel file binary compressed")) << "the question waits for the chooser";

    ASSERT_TRUE(pumpUntil ([&] { return exported.existsAsFile(); }));
    EXPECT_EQ(askedTitles, juce::StringArray ({ "Panel file binary compressed", "License question", "License content" }));
    EXPECT_EQ(licenseIn (exported).toString(), "my licence");
}

TEST_F(LicenseQuestions, export_with_no_writes_the_file_without_a_license)
{
    CtrlrPanel *panel = panelWithResource();
    ASSERT_NE(panel, nullptr);
    const juce::File exported = dir.getChildFile ("out.bpanelz");
    chooserAnswer = { exported };
    buttonsToPress = { "No" };

    panel->savePanelAs (CtrlrEditor::doExportFileZBinRes);
    ASSERT_TRUE(pumpUntil ([&] { return exported.existsAsFile(); }));
    EXPECT_EQ(askedTitles, juce::StringArray ({ "Panel file binary compressed", "License question" }));
    EXPECT_TRUE(licenseIn (exported).isVoid());
}

TEST_F(LicenseQuestions, export_without_resources_asks_nothing)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    const juce::File exported = dir.getChildFile ("out.bpanelz");
    chooserAnswer = { exported };

    panel->savePanelAs (CtrlrEditor::doExportFileZBinRes);
    ASSERT_TRUE(pumpUntil ([&] { return exported.existsAsFile(); }));
    EXPECT_EQ(askedTitles, juce::StringArray ("Panel file binary compressed"));
}

// The panel closed while the licence question is open: nothing is written, nothing is shown.
TEST_F(LicenseQuestions, export_does_nothing_when_the_panel_closed_during_the_question)
{
    CtrlrPanel *panel = panelWithResource();
    ASSERT_NE(panel, nullptr);
    const juce::File exported = dir.getChildFile ("out.bpanelz");
    chooserAnswer = { exported };
    AW::testAnswer() = [this, answer = AW::testAnswer()] (const AW::Request &request) {
        const int result = answer (request);
        if (request.title == "License question")
            processor->getManager().removePanel (processor->getManager().getPanel (0)->getEditor (false));
        return result;
    };

    panel->savePanelAs (CtrlrEditor::doExportFileZBinRes);
    ASSERT_TRUE(pumpUntil ([&] { return askedTitles.contains ("License question"); }));
    pump();
    EXPECT_EQ(processor->getManager().getNumPanels(), 0);
    EXPECT_FALSE(exported.existsAsFile());
    EXPECT_EQ(askedTitles, juce::StringArray ({ "Panel file binary compressed", "License question" }));
}

TEST_F(LicenseQuestions, load_asks_first_and_makes_the_panel_after_yes)
{
    windowResult = 1; // Yes
    EXPECT_EQ(processor->getManager().addPanel (licensedTree(), true), nullptr) << "no panel before the answer";
    EXPECT_EQ(processor->getManager().getNumPanels(), 0);
    EXPECT_EQ(askedTitles, juce::StringArray ("License agreement"));
    EXPECT_EQ(licenseShown, "the terms");

    ASSERT_TRUE(pumpUntil ([&] { return processor->getManager().getNumPanels() == 1; }));
    EXPECT_EQ(askedTitles.size(), 1) << "asked once, not again inside the load";
}

TEST_F(LicenseQuestions, load_with_no_makes_no_panel)
{
    windowResult = 0; // No
    EXPECT_EQ(processor->getManager().addPanel (licensedTree(), true), nullptr);
    pump();
    EXPECT_EQ(processor->getManager().getNumPanels(), 0);
    EXPECT_EQ(askedTitles, juce::StringArray ("License agreement"));
}

// The blocking form, for a plugin's own exported panel: the answer is there when it returns.
TEST_F(LicenseQuestions, blocking_form_returns_the_answer)
{
    const juce::ValueTree resources = licensedTree().getChildWithName (Ids::resourceExportList);
    windowResult = 1;
    EXPECT_TRUE(CtrlrPanelResourceManager::confirmEmbeddedLicense (resources));
    windowResult = 0;
    EXPECT_FALSE(CtrlrPanelResourceManager::confirmEmbeddedLicense (resources));
    EXPECT_EQ(askedTitles, juce::StringArray ({ "License agreement", "License agreement" }));
    EXPECT_EQ(licenseShown, "the terms");

    askedTitles.clear();
    EXPECT_TRUE(CtrlrPanelResourceManager::confirmEmbeddedLicense (juce::ValueTree (Ids::resourceExportList)));
    EXPECT_TRUE(askedTitles.isEmpty()) << "no licence, no question";
}

// CtrlrDialogWindow::showModalDialogAsync(): the window of showModalDialog() owns the content; the
// callback gets the result and the content later, and the content goes with the window.
TEST_F(AlertHelpers, dialog_window_owns_the_content_and_calls_back_later)
{
    struct Content : juce::Component
    {
        bool &deleted;
        explicit Content (bool &d) : deleted (d) { setSize (200, 100); }
        ~Content() override { deleted = true; }
    };
    bool deleted = false;
    auto content = std::make_unique<Content> (deleted);
    juce::Component *contentPtr = content.get();
    juce::Component *shownContent = nullptr;
    AW::testAnswer() = [&] (const AW::Request &request) {
        ++hookCalls;
        asked = request;
        if (auto *w = dynamic_cast<juce::DialogWindow *> (request.window))
            shownContent = w->getContentComponent();
        return 1;
    };

    int calls = 0;
    int got = -1;
    juce::Component *calledWith = nullptr;
    CtrlrDialogWindow::showModalDialogAsync ("Prefs", std::move (content), false, nullptr, [&] (int result, juce::Component &c) {
        ++calls;
        got = result;
        calledWith = &c;
        EXPECT_FALSE(deleted) << "the content still exists in the callback";
    });
    EXPECT_EQ(hookCalls, 1);
    EXPECT_EQ(asked.title, "Prefs");
    EXPECT_EQ(shownContent, contentPtr);
    EXPECT_EQ(calls, 0) << "the callback must not run inside the call";

    EXPECT_TRUE(pumpUntil ([&] { return calls == 1 && deleted; }));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(got, 1);
    EXPECT_EQ(calledWith, contentPtr);
    EXPECT_TRUE(deleted);
}

// Code editor preferences, "Reset": the reset waits for the answer, and nothing happens when the
// window went before it.
TEST_F(LuaEditorDialogs, code_editor_reset_waits_for_the_answer)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    auto editor = std::make_unique<CtrlrLuaMethodEditor> (*panel);
    juce::Value searchTabs;
    const auto resetIn = [] (juce::Component &settings) -> juce::Button * {
        for (auto *child : settings.getChildren())
            if (auto *b = dynamic_cast<juce::TextButton *> (child))
                if (b->getButtonText() == "Reset")
                    return b;
        return nullptr;
    };

    auto settings = std::make_unique<CtrlrLuaMethodCodeEditorSettings> (*editor, searchTabs);
    juce::Button *reset = resetIn (*settings);
    ASSERT_NE(reset, nullptr);
    juce::Slider *size = nullptr;
    for (auto *child : settings->getChildren())
        if (auto *slider = dynamic_cast<juce::Slider *> (child))
            size = slider;
    ASSERT_NE(size, nullptr);
    size->setValue (20.0, juce::dontSendNotification);
    buttonsToPress = { "OK" };
    settings->buttonClicked (reset);
    EXPECT_EQ(askedTitles, juce::StringArray ("Reset Editor"));
    EXPECT_EQ(settings->getFont().getHeight(), 20.0f) << "not reset before the answer";
    pump();
    EXPECT_EQ(settings->getFont().getHeight(), 14.0f);

    settings = std::make_unique<CtrlrLuaMethodCodeEditorSettings> (*editor, searchTabs);
    buttonsToPress = { "OK" };
    settings->buttonClicked (resetIn (*settings));
    settings.reset();
    pump(); // must not touch the deleted settings
    EXPECT_EQ(askedTitles.size(), 2);
}

// A modulator linked to itself: a warning, and the link is reset after it, as before.
TEST_F(AlertCallers, modulator_linked_to_itself_warns_then_resets_the_link)
{
    CtrlrPanel *panel = loadPanel ("fixture_lua_values.panel");
    ASSERT_NE(panel, nullptr);
    CtrlrModulator *m = panel->getModulator ("target");
    ASSERT_NE(m, nullptr);

    m->setProperty (Ids::modulatorLinkedToModulator, m->getName());
    EXPECT_EQ(askedTitles, juce::StringArray ("Modulator link"));
    EXPECT_EQ(m->getProperty (Ids::modulatorLinkedToModulator).toString(), m->getName()) << "reset after the warning";
    pump();
    EXPECT_EQ(m->getProperty (Ids::modulatorLinkedToModulator).toString(), COMBO_NONE_ITEM);
    EXPECT_EQ(askedTitles.size(), 1);
}

// Canvas image import of a file that is no image: a notice, through AW.
TEST_F(AlertCallers, canvas_image_import_of_a_non_image_tells_so)
{
    // Under a UID of this test's own: an imported file is copied into the panel's folder.
    juce::ValueTree tree = CtrlrPanel::openXmlPanel (juce::File::getCurrentWorkingDirectory().getChildFile ("fixtures/fixture_lua_values.panel"));
    ASSERT_TRUE(tree.isValid());
    tree.setProperty (Ids::panelUID, "test-" + juce::Uuid().toString(), nullptr);
    CtrlrPanel *panel = processor->getManager().addPanel (tree, true);
    ASSERT_NE(panel, nullptr);
    struct RemoveFolder
    {
        juce::File folder;
        ~RemoveFolder() { folder.deleteRecursively(); }
    } removeFolder { panel->getPanelDirectory() };
    juce::File notAnImage = dir.getChildFile ("not_an_image.txt");
    ASSERT_TRUE(notAnImage.replaceWithText ("text"));

    panel->getEditor (false)->getCanvas()->importImage (notAnImage);
    ASSERT_TRUE(pumpUntil ([&] { return askedTitles.size() == 1; }));
    EXPECT_EQ(askedTitles[0], "Image import");
}

// The value-tree editor's F2 rename (the Lua method editor's method tree): the provider renames
// the item in the callback, and nothing happens when the tree view went before the answer.
TEST_F(LuaEditorDialogs, value_tree_rename_renames_in_the_callback)
{
    struct Provider : CtrlrValueTreeEditorLookProvider
    {
        juce::StringArray renamedTo;
        const bool canBeRenamed (const juce::ValueTree &) const override { return true; }
        const bool renameItem (const juce::ValueTree &, const juce::String &newName) const override
        {
            const_cast<Provider *> (this)->renamedTo.add (newName);
            return true;
        }
        void handleAsyncUpdate() override {}
    } provider;
    juce::ValueTree tree ("luaMethod");
    tree.setProperty (Ids::name, "before", nullptr);

    auto view = std::make_unique<juce::TreeView>();
    auto *item = new CtrlrValueTreeEditorItem (provider, tree);
    view->setRootItem (item);
    typed = "after";
    const juce::KeyPress f2 (65649); // the key code the item checks
    item->keyPressed (f2);
    EXPECT_EQ(askedTitles, juce::StringArray ("Rename item"));
    EXPECT_EQ(tree.getProperty (Ids::name).toString(), "before") << "renamed only in the callback";
    pump();
    EXPECT_EQ(provider.renamedTo, juce::StringArray ("after"));
    EXPECT_EQ(tree.getProperty (Ids::name).toString(), "after");

    typed = "never";
    item->keyPressed (f2);
    view->deleteRootItem();
    view.reset();
    pump();
    EXPECT_EQ(provider.renamedTo.size(), 1);
    EXPECT_EQ(tree.getProperty (Ids::name).toString(), "after");
}
