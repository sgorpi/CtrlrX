#include "test_ProcessorFixture.h"

#include "CtrlrManager.h"
#include "CtrlrPanel.h"
#include "CtrlrModulator.h"
#include "CtrlrLuaManager.h"
#include "Methods/CtrlrLuaMethodManager.h"
#include "Methods/CtrlrLuaMethod.h"

#include <cmath>

/**
 * Panel Lua binding surface (MIDI-independent) guard.
 *
 * The Lua<->MIDI path is covered by test_ProcessorMidiLua.cpp. This suite pins down the generic
 * C++<->Lua marshalling: the injected global objects, the CtrlrPanel / CtrlrModulator / 
 * CtrlrComponent getters & setters, the load lifecycle hooks, the value-change callback 
 * argument marshalling, and Lua error handling. 
 *
 * Observation seam (same black box as the other suites, no production code touched): a fixture
 * panel's Lua writes a numeric outcome into a result modulator via mod:setValue(n, true); the
 * test reads it back with processor->getParameter(vstIndex). All result modulators span 0..127,
 * so getParameter returns value/127 and readInt() recovers the integer.
 *
 * The self-test fixtures (bindings, gui) walk a numbered checklist and, on the FIRST failure,
 * store that check's id in a `first-fail` modulator and bump a `fail-count` modulator. The C++
 * test asserts fail-count == 0 and prints first-fail, so a broken binding names itself.
 *
 * Fixtures (generated, see Tests/fixtures/fixture_lua_*.panel):
 *  - fixture_lua_lifecycle   : luaPanelBeforeLoad / luaPanelLoaded -> counters + mode arg
 *  - fixture_lua_bindings    : run -> runSelfTest over globals + panel/modulator getters
 *  - fixture_lua_gui         : run -> runGuiTest over component getters/setters (nil-guarded)
 *  - fixture_lua_valuechange : src -> onChange(mod, value, source) marshalling
 *  - fixture_lua_invalid     : a method with a syntax error -> compiles invalid at load
 */

class Lua : public ProcessorInstance
{
protected:
    // Result modulators all span 0..127, so getParameter (normalized) * 127 recovers the integer
    // the Lua stored via setValue(n, true).
    int readInt(int vstIndex)
    {
        return (int) std::lround(processor->getParameter(vstIndex) * 127.0f);
    }

    // Changing a modulator's value fires its luaModulatorValueChange method. The callback runs from
    // the modulator's async update on the message thread, so the loop must be pumped afterwards.
    void triggerLua(int vstIndex, float value)
    {
        processor->prepareToPlay(44100, BLOCK_SIZE);
        processor->setParameter(vstIndex, value);
        processor->processBlock(buffer, midiMessages);
        midiMessages.clear();
        pumpInputThreads(300);
    }
};

// ----------------------------------------------------------------------------------------------
// Load lifecycle
// ----------------------------------------------------------------------------------------------

/**
 * The panel load hooks fire exactly once. luaPanelBeforeLoad bumps a Lua global; luaPanelLoaded
 * (invoked from bootstrapPanel, after modulators exist) publishes both counters and stores its
 * uint8 `mode` argument -- exercising the C++->Lua call and the arg marshalling on the load hook.
 */
TEST_F(Lua, panel_load_fires_lifecycle_callbacks_once)
{
    ASSERT_NE(loadPanel("fixture_lua_lifecycle.panel"), nullptr);
    processor->prepareToPlay(44100, BLOCK_SIZE);
    pumpInputThreads(300);

    EXPECT_EQ(readInt(1), 1) << "luaPanelBeforeLoad should fire exactly once";
    EXPECT_EQ(readInt(2), 1) << "luaPanelLoaded should fire exactly once";
    EXPECT_EQ(readInt(3), (int) processor->getManager().getInstanceMode())
        << "luaPanelLoaded(mode) should receive the instance mode as its argument";
}

// ----------------------------------------------------------------------------------------------
// Global objects + panel/modulator bindings
// ----------------------------------------------------------------------------------------------

/**
 * The injected global objects and the CtrlrPanel / CtrlrModulator getter+setter surface all
 * dispatch and round-trip. runSelfTest walks checks 1..22; fail-count == 0 means every binding
 * worked, and first-fail names the first that didn't.
 */
TEST_F(Lua, lua_global_objects_and_panel_bindings_available)
{
    ASSERT_NE(loadPanel("fixture_lua_bindings.panel"), nullptr);
    triggerLua(1, 0.5f); // drive `run` -> runSelfTest

    EXPECT_EQ(readInt(2), 0)
        << "a Lua binding self-test check failed; first failing check id = " << readInt(3);
}

// ----------------------------------------------------------------------------------------------
// GUI component accessors
// ----------------------------------------------------------------------------------------------

/**
 * The CtrlrComponent getters/setters reachable from Lua (getComponent, isVisible/setVisible,
 * getWidth/getHeight/getX/getY, setSize) dispatch and return sensible values. If components are
 * not instantiated in this headless harness the fixture records a skip (first-fail = 99) with
 * fail-count 0, so this still confirms the accessors compile and dispatch without crashing.
 */
TEST_F(Lua, lua_gui_component_getters_setters_return_sensible_values)
{
    ASSERT_NE(loadPanel("fixture_lua_gui.panel"), nullptr);
    triggerLua(1, 0.5f); // drive `run` -> runGuiTest

    const int firstFail = readInt(3);
    EXPECT_EQ(readInt(2), 0)
        << "a Lua GUI-accessor check failed; first failing check id = " << firstFail;
    // if (firstFail == 99)
    //     std::cout << "note: GUI components nil headless; GUI value checks were skipped" << std::endl;
    EXPECT_NE(firstFail, 99) << "note: GUI components nil headless; GUI value checks were skipped" << std::endl;
}

// ----------------------------------------------------------------------------------------------
// Value-change callback marshalling
// ----------------------------------------------------------------------------------------------

/**
 * A host-driven value change delivers onChange(mod, value, source) exactly once with the correct
 * value and source. setParameter routes through changedByHost, which is the one source that is NOT
 * suppressed, so the callback fires. Exercises call(CtrlrModulator*, double, uint8).
 */
TEST_F(Lua, lua_value_change_callback_delivers_correct_args)
{
    ASSERT_NE(loadPanel("fixture_lua_valuechange.panel"), nullptr);
    processor->prepareToPlay(44100, BLOCK_SIZE);

    processor->setParameter(1, 40.0f / 127.0f); // drive `src` to raw value 40 from the host
    processor->processBlock(buffer, midiMessages);
    midiMessages.clear();
    pumpInputThreads(300);

    EXPECT_EQ(readInt(2), 1) << "onChange should fire exactly once per host value change";
    EXPECT_EQ(readInt(3), 40) << "onChange should receive the changed value";
    EXPECT_EQ(readInt(4), (int) CtrlrModulatorValue::changedByHost)
        << "onChange should receive changedByHost (1) as its source argument";
}

// ----------------------------------------------------------------------------------------------
// Lua error handling
// ----------------------------------------------------------------------------------------------

/**
 * A method with a syntax error fails to compile at load and is marked invalid, without preventing
 * the panel from loading. Guards the setCodeInternal -> runCode(false) -> setValid(false) path
 * that a binding-layer swap must preserve.
 */
TEST_F(Lua, lua_invalid_method_self_disables_without_crash)
{
    CtrlrPanel* panel = loadPanel("fixture_lua_invalid.panel");
    ASSERT_NE(panel, nullptr) << "panel with an invalid Lua method should still load";

    CtrlrLuaMethod* broken = panel->getCtrlrLuaManager().getMethodManager().getMethodByName("broken");
    ASSERT_NE(broken, nullptr) << "the broken method should still exist in the method manager";
    EXPECT_FALSE(broken->isValid()) << "a method with a syntax error should be marked invalid at load";
}
