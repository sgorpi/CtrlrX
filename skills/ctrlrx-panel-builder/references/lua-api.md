# Lua API for Panels

CtrlrX embeds **Lua 5.1** (1-based arrays, `..` concatenation, `obj:method()` colon-call syntax).
This page is the curated API panel authors actually use. The build is the final authority — the
bindings live in `Source/Lua/CtrlrLuaManager.cpp`, `Source/Lua/CtrlrComponentLuaRegistration.cpp`,
and `Source/MIDI/CtrlrMidiMessage.cpp`; the running app's `how()` / `what(obj)` introspection prints
the exact, build-correct list.

## How Lua attaches to a panel (recap)

Methods are stored in the panel's `<luaManager>` and **referenced by name** from callback
attributes. The inner `function` name must match `luaMethodName`, and the code must be XML-escaped
inside `luaMethodCode` (use `scripts/panel_tool.py escape`). A callback attribute set to a name with
no matching method simply never fires. See `panel-file-format.md` for the XML, and the callback
catalogue below for which attribute to set.

Two mechanics that shape how you structure panel Lua:

- **A method body is a chunk executed at panel load**, in document order, not a function definition
  that CtrlrX wraps for you. So top-level code in a method runs at load, and a `<luaMethod>` may
  define constants and several helpers while defining no function of its own name.
- **The code can live in an external `.lua` file** (`luaMethodSource="1"`), which is what makes panel
  Lua testable under a plain `lua5.1` interpreter and reviewable in version control. Both are
  detailed in `panel-file-format.md`.

## Global objects (available in every method)

| Global | Type | What it is |
|---|---|---|
| `panel` | CtrlrPanel | The panel — your main entry point. |
| `utils` | CtrlrLuaUtils | Dialogs, encoding, version helpers. |
| `timer` | CtrlrLuaMultiTimer | Repeating callbacks. |
| `devices` | CtrlrMIDIDeviceManager | Enumerate / open MIDI devices. |
| `resources` | CtrlrPanelResourceManager | Embedded images / fonts / data. |
| `native`, `afm`, `atc`, `converter` | — | OS-native + audio helpers. |

Debug output: `console("...")` (prints to the Lua console). `how()` lists every bound class;
`what(obj)` lists an object's methods.

## CtrlrPanel

| Method | Purpose |
|---|---|
| `getModulatorByName(name)` / `getModulator(name)` | Get a modulator by **Name** (case-sensitive). Returns `nil` if no match — guard before chaining. |
| `getModulatorByIndex(i)` / `getNumModulators()` | Index access / count. |
| `getComponent()` / `getCanvas()` / `getPanelEditor()` | Root component / canvas / editor. |
| `sendMidiMessageNow(msg)` | Send a `CtrlrMidiMessage` immediately (a bare hex string also works, but only from 5.6.35 - see the note below). |
| `sendMidi(msg, ts)` | Send with timestamp (`CtrlrMidiMessage`, `MidiMessage`, or `MidiBuffer`). |
| `getGlobalVariable(i)` / `setGlobalVariable(i, v)` | Panel-wide integer slots. |
| `getProgramState()` / `setProgramState(...)` | Capture / restore all values (a patch). |
| `isLoading()` / `getRestoreState()` / `setRestoreState(...)` / `isRestoring()` | Load-time flags. |

## CtrlrModulator

`local m = panel:getModulatorByName("filterCutoff")`

**Values** — `getValue()`/`getValueInt()`, **`setValue(v, force[, mute])`**; mapped variants
`getValueMapped()`/`setValueMapped()`; explicit-raw `getValueNonMapped()`/`setValueNonMapped()`.
Older panels use `getModulatorValue`/`setModulatorValue` (aliases of get/setValue).

> **Gotcha — `setValue` needs ≥2 args.** Only `setValue(int, bool)` and `setValue(int, bool, bool)`
> are bound to Lua; a one-arg `m:setValue(v)` raises *"No matching overload found"* and **disables the
> whole calling method**. Pass `m:setValue(v, force[, mute])` — `force=true` applies even if the value
> is unchanged (commits the display); `mute=true` suppresses the modulator's *own* outgoing MIDI (not
> its value-change Lua callback). Verified in `Source/Lua/CtrlrLuaManager.cpp`.
**Range** — `getMinModulatorValue()`/`getMaxModulatorValue()` (+`Int`), and `getMin/MaxMapped()`,
`getMin/MaxNonMapped()`.
**Identity** — `getName()`/`getModulatorName()`/`getLuaName()`, `getComponent()`,
`getMidiMessage()`, `getVstIndex()`.

```lua
local m = panel:getModulatorByName("x"); if m then m:setValue(0, true) end  -- guard nil; ≥2 args
```

> **Gotcha — `getName()` returns a wrapped `juce::String`, not a Lua string.** All three name
> getters (`getName`/`getModulatorName`/`getLuaName`) bind to `CtrlrModulator::getName`, which
> returns a `juce::String` marshalled as **userdata**. So `m:getName() == "x"` is *always false*
> (userdata≠string), `tostring(m:getName())` is not the name, and two calls aren't `==` to each
> other. To compare modulator identity use the native-int `getVstIndex()`; the returned String is
> still accepted anywhere a `String` parameter is expected (e.g. `panel:getModulatorByName(m:getName())`
> round-trips). Value/range getters (`getValueInt`, `getMaxModulatorValue`, `getVstIndex`, …) return
> native numbers and compare fine.

## CtrlrComponent

`local c = m:getComponent()`

| Group | Methods |
|---|---|
| Geometry | `getX/Y/Width/Height()`, `getBounds()`, `setBounds()`, `setSize()`, `getRect()` |
| Value | `getValue()`/`setValue()`, `getComponentValue()`/`setComponentValue()`, `getComponentMidiValue()`/`setComponentMidiValue()`, `getMidiValue()`/`setMidiValue()` |
| Text | `getComponentText()`/`setComponentText()`, `getTextForValue()` |
| Range | `getMinimum()`, `getMaximum()` |
| Visibility / paint | `isVisible()`, `setVisible()`, `repaint()` |
| Mouse / keys | `click()`, `mouseUp()`, `mouseEnter()`, `mouseExit()`, `keyPressed()`, `isMouseOver()` |
| Owner | `getOwner()` (the modulator) |

> ⚠️ Gotcha — **to show/hide a control from Lua, set the `componentVisibility` tree property, not
> `setVisible()`.** Both hide the control on screen (`CtrlrComponent` reacts to the property by calling
> `setVisible()` itself — `CtrlrComponent.cpp`), but only the property is *state*: `setVisible()` touches
> the live JUCE component and leaves the tree untouched, so the change neither persists nor is visible
> to anything reading the panel. In particular `CtrlrPanelProbe` reports a modulator's `visible` by
> reading the tree (`probe_Report.cpp`: `comp.getProperty(Ids::componentVisibility, true)`) — so a page
> or sub-type group toggled with `setVisible()` still reports `visible: true` in `finalState`, and every
> headless assertion about which group is showing quietly passes no matter what the panel does.
>
> ```lua
> local c = panel:getModulatorByName(name):getComponent()
> if c ~= nil then c:setPropertyInt("componentVisibility", on and 1 or 0) end
> ```
>
> This is the pattern for sub-type / per-voice groups: register `{selector, value, names}` groups and
> re-apply them on every value change *and* after a page switch (a control on a hidden layer may not be
> realized yet, so an apply at boot can silently no-op).

**Component-specific** (registered in `CtrlrComponentLuaRegistration.cpp`):
- Toggle/Image button: `getToggleState()`, `setToggleState()`, `getValueMap()`
- Label/LCD: `setText()`, `appendText()`, `getText()`
- Combo: `getSelectedId()`/`setSelectedId()`, `getSelectedItemIndex()`/`setSelectedItemIndex()`, `getText()`/`setText()`, `getOwnedComboBox()`

> **Gotcha — `uiComboSelectedIndex` / `uiComboSelectedId` are tree properties CtrlrX never
> writes.** They read `-1` for *every* value, including one that is perfectly in range, so
> `comp:getProperty("uiComboSelectedIndex")` cannot tell you what a combo is showing — and it
> returns the stored value as a **string**, not a number. This is the same shape as the
> `componentVisibility` / `isVisible()` / effective-visibility split: the tree property is not
> the live state. Ask the component: `panel:getComboComponent(name):getSelectedItemIndex()`
> returns a real int (`-1` genuinely means "nothing selected", which is what a value outside
> the value map gives you). Prefer it to `getText()`, which hands back a juce `String` as
> userdata — concatenating one segfaults CtrlrX.
- Slider: `getOwnedSlider()`
- ListBox: `getNumRows()`, `updateContent()`, `selectRow()`, `getSelectedRow()`, `getSelectedRows()`
- FileListBox: `getSelectedFile()`, `refresh()`, `getNumSelectedFiles()`

## CtrlrMidiMessage & sending MIDI

Construct from a hex string (`"B0 4A 7F"`), a JUCE `MidiMessage`, a `MemoryBlock`, or a Lua table of
bytes. Methods: `getMidiMessageType()`/`setMidiMessageType()`, `getChannel()`/`setChannel()`,
`getNumber()`/`setNumber()`, `getValue()`/`setValue()`, `getSize()`, `getData()`/`getLuaData()`,
`toString()`.

Message-type constants (Lua): `CC`, `Aftertouch`, `ChannelPressure`, `NoteOn`, `NoteOff`, `SysEx`,
`Multi`, `ProgramChange`, `PitchWheel`, `None`, `MidiClock`, `MidiClockContinue`, `MidiClockStop`,
`MidiClockStart`, `ActiveSense`.

```lua
-- portable form - works on every build. Channel is in the status byte (ch1 = 0x90).
local status, note, vel = 0x90, 60, 100
panel:sendMidiMessageNow(CtrlrMidiMessage(MidiMessage(status, note, vel, 0.0)))

panel:sendMidiMessageNow(CtrlrMidiMessage("F0 7D 12 40 F7"))  -- SysEx, hex string
local mm = CtrlrMidiMessage("90 00 00")
mm:setChannel(1); mm:setNumber(note); mm:setValue(vel)        -- or set fields
panel:sendMidiMessageNow(mm)
```

> **Two convenient forms are newer than they look — check the user's build version first.**
> `panel:sendMidiMessageNow("B0 4A 7F")` (a bare hex *string*, i.e. `sendMidiMessageNow_String`)
> and `CtrlrMidiMessage({ 0xB0, 74, 127 })` (a Lua *table* of bytes) were both added in
> **5.6.35**. On an older build the string form throws
> `No matching overload found, candidates: void sendMidiMessageNow(CtrlrPanel&, CtrlrMidiMessage&)`
> — that single-candidate list is the tell. 5.6.35 also *rewrote* the
> `CtrlrMidiMessage(const String&)` hex constructor, so hex parsing itself differs between
> builds; on 5.6.34 the sanitiser is `replaceCharacter(' ', 0)`, which truncates at the
> first space.
>
> `CtrlrMidiMessage(const MidiMessage&)` and `CtrlrComponent::getOwner` date to 2014 and the
> `MidiMessage(int, int, int, double)` binding to 2015, none touched since — so the
> `CtrlrMidiMessage(MidiMessage(status, d1, d2, 0.0))` form above is the safe default for a
> panel that has to run on whatever CtrlrX the user has. It also sidesteps hex parsing
> entirely. Check a binary with
> `strings -a <so> | grep -oE '^5\.6\.[0-9]+$'` and
> `nm -C <so> | grep sendMidiMessageNow_String`.
>
> **A headless pass does not prove this.** `CtrlrPanelProbe` is built from the current
> source, so it happily runs bindings the user's installed plugin lacks — a panel can be
> 100% green in the probe and throw on load in their DAW.

## utils / timer / resources (quick index)

- `utils`: dialogs (`warnWindow`, `infoWindow`, `questionWindow`, `askForTextInputWindow`,
  `openFileWindow`, `saveFileWindow` — see *Files and binary data* below for their return and
  cancel semantics), encoding (`base64_encode/decode`, `packDsiData/unpackDsiData`,
  `get8bitSigned`, `get16bitSigned`), `getMidiInputDevices()`, `getVersionString()`.
- `timer`: `setCallback(id, fn)`, `startTimer(id, ms)`, `stopTimer(id)`, `isTimerRunning(id)`,
  `isRegistered(id)`, `getTimerInterval(id)`. It wraps JUCE `MultiTimer`, so **every timer repeats
  — there is no one-shot**. For a fire-once action (an indicator that decays, a deferred send) the
  callback must stop its own timer:

  ```lua
  timer:setCallback(11, function(id)
      timer:stopTimer(id)          -- first line, or it repeats forever
      ledOff("led-trigger-adsr")
  end)
  timer:startTimer(11, 120)
  ```

  Re-arming is `stopTimer` then `startTimer` again — that is how you build a retriggerable decay.
  Ids are yours to allocate; keep them in one table so two features can't collide.
  (Verified headlessly: the callback above fires exactly once.)
- `resources`: `getResource(name)`, `getResourceAsImage(name)`, `getResourceAsFont(name)`; a
  resource exposes `asImage()`, `asText()`, `asData()`, `asXml()`, `getName()`, `getSize()`.

## Changing a component's look at runtime

Any component property can be written from Lua, which is how you animate a panel — light an
indicator, recolour a control to show state, hide something conditionally:

```lua
local c = panel:getModulatorByName("led-trigger-adsr"):getComponent()
c:setProperty("uiLabelBackgroundColour", "ffff3000", false)   -- 3rd arg: add to undo history
```

The write sticks and reads back via `c:getProperty(...)`. Two cautions:

- **Property names are per-`uiType`.** The colour attribute of a `uiLabel` is not the one a
  `uiGroup` or `uiButton` uses. Get the exact name from the type's source in
  `Source/UIComponents/CtrlrComponents/`, or by dumping a real component with
  `panel_tool.py dump FILE modulator:NAME` — a wrong name fails **silently**.
- **Assert it headlessly.** `CtrlrPanelProbe`'s `eval` can set and read back the property in the
  panel's own `lua_State`, so a typo'd property name is caught by a test rather than by squinting
  at the GUI.

Note that `comp:setVisible(false)` does **not** write the `componentVisibility` tree property —
see the three visibility fields in `SKILL.md`.

## Callback hooks — what to attach, and its signature

Set these attribute names to a method name. The parameter list is **pre-wired** per callback; write
the body, don't change the params. Names come from `Source/Resources/XML/CtrlrLuaMethodTemplates.xml`.

**Panel-level** (attributes on `<panel>`):

| Callback | Fires when | Signature |
|---|---|---|
| `luaPanelBeforeLoad` / `luaPanelLoaded` / `luaPanelResourcesLoaded` | Panel lifecycle | `function()` |
| `luaPanelSaved` | Panel saved | `function(saveType, file)` |
| `luaPanelMidiReceived` | Any MIDI in | `function(midi)` |
| `luaPanelMidiMultiReceived` | Multi-message in | `function(multi)` |
| `luaPanelModulatorValueChanged` | Any modulator changed | `function(mod, value)` |
| `luaPanelProgramChanged` | Program change | `function(prog, bankLsb, bankMsb)` |
| `luaPanelGlobalChanged` | Global var changed | `function(index, value)` |
| `luaPanelPaintBackground` | Custom background | `function(canvas, g)` |
| `luaPanelSaveState` / `luaPanelRestoreState` | Persist custom state | `function(tree)` |
| `luaAudioProcessBlock` | Audio block (plugin) | `function(midiBuffer, posInfo)` |

**Modulator-level** (attributes on `<modulator>`):

| Callback | Fires when | Signature |
|---|---|---|
| `luaModulatorValueChange` | This modulator changed | `function(mod, value, source)` |
| `luaModulatorGetValueForMIDI` | Custom value → MIDI | `function(mod, value)` → MIDI value |
| `luaModulatorGetValueFromMIDI` | Custom MIDI → value | `function(mod, midiValue)` → value |

The `source` arg (use it to avoid MIDI feedback loops — e.g. don't re-send when the change *came
from* MIDI in): `0` initial, `1` host, `2` MIDI in, `3` controller, `4` GUI, `5` Lua, `6` program,
`7` link, `8` unknown. (Enum `LastChangeSource` in `Source/Core/CtrlrModulator/CtrlrModulatorProcessor.h`.)

> **Gotcha — `luaModulatorValueChange` fires on a *change*, so a no-op set runs nothing.**
> Driving a control to the value it already holds does not call it — on any component type,
> including a `uiCombo`, which *does* still re-emit its MIDI. Two consequences. A test that sets a
> selector to the value the panel was saved with reads the state the callback would have produced
> as if the callback were broken, so **prime with a different value first and discard that step**.
> And on a **cold load** nothing fires at all, so a hook that derives state from a selector (a
> dependent range, a visibility set) leaves the panel inconsistent until the user touches it —
> call it once from the panel's `luaPanelCreated`/init method with the restored value. Measured
> with `CtrlrPanelProbe`.

> **Gotcha — `luaModulatorValueChange` fires ASYNC, so guard the echo with `source`, not a flag.**
> The callback runs from `CtrlrModulatorProcessor::handleAsyncUpdate()` (via `triggerAsyncUpdate`),
> *after* the `setValue` call that caused it returns. So a boolean like `syncing = true … setValue …
> syncing = false` set/cleared synchronously around a bulk load is already `false` when the callbacks
> land — it can't suppress them. Instead gate on the race-free `source`: a programmatic `setValue`
> arrives as `5` (Lua), a real knob turn as `4` (GUI). To push MIDI only for genuine user edits,
> `if source == 4 then …`. (This is the reliable way to bulk-load modulators from a device dump
> without echoing every field back out.)

**Component-level** (attributes on `<component>`): mouse — `uiCustomMouseDownCallback`,
`…MouseUpCallback`, `…MouseDragCallback`, `…MouseMoveCallback`, `…MouseEnter/ExitCallback`,
`…MouseDoubleClickCallback`, `…MouseWheelMoveCallback`; keys — `uiCustomKeyDownCallback`; paint —
`uiCustomPaintCallback(component, g)`, `uiCustomResizedCallback`; per-type —
`uiTabsCurrentTabChanged` (called `(modulator, tabIndex)` whenever `uiTabsCurrentTab` changes,
`CtrlrTabsComponent.cpp:308` -- the natural hook for showing a canvas layer per tab), `uiListBoxItemClicked/DoubleClicked`, `uiFileListFileClicked`,
`uiWaveFormSourceChangedCallback`; drag&drop — `uiCustomItemDropped/Drag*Callback`. (Legacy mouse
names `componentLuaMouseDown/Up/Drag/...` also exist.)

### Bound but unusable: `panel:getInputComparator()`

It is registered as a `CtrlrPanel` method, but `CtrlrMidiInputComparator::wrapForLua` is never
called from `CtrlrLuaManager`, so calling it raises
`std::runtime_error: 'Trying to use unregistered class'`. `rehashComparator()`, `dumpTables()`
and `addMatchTarget()` are therefore **unreachable from panel Lua** — you cannot rebuild or
inspect the MIDI input match table at runtime. Verified 2026-09-15.

## JUCE classes exposed to Lua

CtrlrX exposes a large slice of JUCE: `File`, `MemoryBlock`, `Colour`, `Graphics`, `Image`, `Font`,
`Path`, `Rectangle`, `Justification`, `MidiMessage`, `MidiBuffer`, `Random`, `Range`, `String`,
`StringArray`, `ValueTree`, `XmlElement`, `PopupMenu`, `AlertWindow`. Example in a paint callback:
`g:setColour(Colour(0xFF303030))`.

**Do not assume the JUCE signature is what's bound.** The wrappers in `Source/Lua/JuceClasses/` change
constructors, argument counts and indexing, and luabind's overload resolution can silently pick the
wrong one. The traps below are measured, not inferred.

### juce `String` returns: sometimes a Lua string, sometimes userdata

`Source/Core/stdafx_luabind.h` specializes `default_converter` for **`String`** and **`String const&`**
only — so the C++ return type decides what Lua gets:

| C++ return type | Lua sees | Example |
|---|---|---|
| `String` (by value) | **Lua string** | `File:getFileName()`, `utils.askForTextInputWindow()` |
| `const String&` | **Lua string** | `File:getFullPathName()` |
| `const String` (top-level const) | **`String` userdata** | `CtrlrModulator:getName()`, `:getLuaName()` |

A `String` userdata **never compares equal** to a Lua string, and `tostring()` on it yields
`"String object: 0x…"`. Worse, **`"" .. theUserdata` kills the process** — not a catchable Lua
error, and it takes the host down with it (measured under `CtrlrPanelProbe` 2026-09-12: exit 139,
SIGSEGV; SIGABRT has also been seen, so do not rely on which). `string.format("%s", s)` does *not*
crash but yields the useless `"String object: 0x…"`. To get text out: `s:toUTF8():getAddress()` →
Lua string. To key a table by modulator, prefer the integer `mod:getVstIndex()`.

```lua
local name = mod:getName()                  -- userdata!
if name == "cutoff" then end                -- ALWAYS false
local text = name:toUTF8():getAddress()     -- "cutoff" -- a real Lua string
```

**Reading a component property: use `getProperty`, not `getPropertyString`.** `getProperty` returns
`const var&` (`CtrlrComponent.h:85`) and `var` converts to a native Lua value — string, number or
bool — so it is the clean way to read UI state back, especially from a probe `eval`, where the
result must survive conversion to JSON. `getPropertyString` hands back a `String` userdata carrying
every hazard above.

```lua
panel:getComponent("myLabel"):getProperty("uiLabelText")        -- "hello" -- a Lua string
panel:getComponent("myLabel"):getPropertyString("uiLabelText")  -- String userdata
panel:getLabel("myLabel"):getText()                             -- String userdata
```

### Files and binary data

Ground truth: `Source/Lua/CtrlrLuaUtils.cpp`, `JuceClasses/LCore.cpp` (File), `JuceClasses/LMemory.cpp`.

- `utils.openFileWindow(title, file, patterns, useNative)` / `saveFileWindow(...)` → a `File`.
  **Cancel returns `File()`** — test with `f:isValid()`, not `nil`.
- `File(path)` is bound, but JUCE asserts on a *relative*, non-empty path. `File("")` is the safe
  "no initial directory" value to hand a dialog.
- `f:loadFileAsData(mb)` **appends** to `mb` (JUCE `readIntoMemoryBlock` → `MemoryOutputStream(block,
  true)`). Always pass a **fresh** block, or you silently concatenate onto the old contents.
- `f:replaceWithData(mb)` / `f:appendData(mb)` take a `MemoryBlock` (the raw-pointer overload is
  unbound). Both return a bool — check it.
- **`MemoryBlock(1366)` gives an EMPTY block** (`getSize() == 0`): luabind resolves the single-argument
  constructor to the `luabind::object` (table) overload, not `int` — in current builds
  `.def(constructor<const int>())` is commented out outright (`LMemory.cpp`, "Useless and returns size
  x2"). You get a 0-byte file and no error. Use `MemoryBlock()` + `setSize(n, true)`,
  `MemoryBlock({b1, b2, …})` from a 1-based table, or the two-arg `MemoryBlock(n, true)`.
  **But never pre-size a block you are about to `loadFileAsData` into** — that appends (above), so a
  sized block comes back double length with `n` leading zeros and every `getByte()` check reads the
  padding. For a file read `MemoryBlock()` *is* the correct call. Measured: `MemoryBlock(434)` → 0 ·
  `MemoryBlock(434, true)` → 434 · `MemoryBlock(434, true)` + a 434-byte `loadFileAsData` → **868**,
  byte 0 = `0x00`.
- `mb:setSize(n)` raises *"No matching overload"* — luabind exposes no default args, so pass both:
  `mb:setSize(n, true)`.
- `mb:getByte(i)` / `mb:setByte(i, v)` are **0-based**, and `mb:getSize()` returns a **double**.
  `setByte`'s bounds test is `size >= pos`, so `setByte(n, v)` on an `n`-byte block writes one past
  the end — stay within `0 .. n-1`.
- `utils.askForTextInputWindow(title, msg, initial, label, isPassword, okText, cancelText)` returns the
  literal string **`"-1"`** on cancel (so that value cannot be entered through the dialog).

```lua
-- write a 1-based Lua byte array to disk
local mb = MemoryBlock()
mb:setSize(#bytes, true)                                -- both args required
for i = 1, #bytes do mb:setByte(i - 1, bytes[i]) end     -- 0-based
local f = File(path)
if not f:replaceWithData(mb) then console("write failed: " .. path) end

-- read it back
local inMb = MemoryBlock()                              -- fresh: loadFileAsData appends
if f:loadFileAsData(inMb) then
    local n = math.floor(tonumber(inMb:getSize()))      -- getSize() returns a double
    for i = 1, n do bytes[i] = inMb:getByte(i - 1) end
end
```

### Painting a `uiCustomComponent`

Ground truth: `Source/Lua/JuceClasses/LGraphics.cpp`,
`Source/UIComponents/CtrlrComponents/CtrlrCustomComponent.cpp`.

- **`Graphics` rectangle calls are bound int-only, line calls float-only.** `fillRect` and
  `drawRect` expose just the `(int,int,int,int[,int])` overloads (`LGraphics.cpp:416/424`), while
  `drawLine` is `(float,float,float,float[,float])` (`:438`). Cell geometry derived from
  `comp:getWidth()/N` is fractional, so **floor every rectangle argument** — an unfloored value
  hits luabind's overload resolution, not a clean error.
- All three mouse callbacks take `f(comp, mouseEvent)` with component-relative `e.x`/`e.y`
  (`CtrlrCustomComponent.cpp:114/125/136`). A plain **Lua table `{x=…, y=…}` works in their place**,
  which is how you drive a drawing surface from `CtrlrPanelProbe` (there is no mouse action):
  `eval` → `fw_paint_down(comp, {x=40, y=10})`.
- **`uiCustomDragAndDropContainer="1"` hijacks `mouseDrag`**: the DnD *start* callback runs before
  your `uiCustomMouseDragCallback` on every drag. Leave it `0` for a paint-by-dragging surface.
- Text: `g:drawFittedText(s, x, y, w, h, Justification(Justification.centred), lines, 1.0)`. A label
  centred on a boundary at `y=0` gets clipped — clamp it inside the component yourself.

## A complete small example

A combo `lfoShape` that updates a label `lfoLabel` and logs on change. Method body (before escaping):

```lua
lfoShapeChanged = function(mod, value)
    local names = { [0]="Sine", [1]="Triangle", [2]="Saw", [3]="Square" }
    local text = names[value] or "?"
    local lbl = panel:getModulatorByName("lfoLabel")
    if lbl then lbl:getComponent():setComponentText(text) end
    console(string.format("LFO shape -> %s (%d)", text, value))
end
```

Store it as a `<luaMethod luaMethodName="lfoShapeChanged" luaMethodCode="…escaped…">` and set
`luaModulatorValueChange="lfoShapeChanged"` on the `lfoShape` modulator.
