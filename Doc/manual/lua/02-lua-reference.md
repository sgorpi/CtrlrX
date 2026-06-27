[← Lua Guide](01-lua-guide.md) | [Manual Index](../README.md)

---

# Lua API Reference

> **TL;DR**
> - Scripts get ready-made **globals**: `panel`, `utils`, `timer`, `devices`, `resources`, and more.
> - This page is a **curated** reference of the classes panel authors actually use, plus the callback hooks.
> - For the exhaustive, always-correct list, ask the running app: `how()` and `what(obj)` ([Chapter 9](../09-debugging.md)).
> - Non-`Ctrlr…` classes (File, Colour, Graphics, …) are JUCE classes — see the [JUCE API docs](https://docs.juce.com/master/index.html).

[← Lua Guide](01-lua-guide.md) · [Manual Index](../README.md)

---

> ⚠️ **Accuracy note.** These names were taken from the luabind registrations in this source tree
> (`Source/Lua/CtrlrLuaManager.cpp`, `Source/MIDI/CtrlrMidiMessage.cpp`,
> `Source/UIComponents/.../CtrlrComponent.cpp` and `CtrlrComponentLuaRegistration.cpp`,
> `Source/Lua/CtrlrLuaUtils.cpp`). Your build is the final authority — confirm with `what(obj)`.

## Contents

- [Global objects](#global-objects)
- [CtrlrPanel](#ctrlrpanel)
- [CtrlrModulator](#ctrlrmodulator)
- [CtrlrComponent](#ctrlrcomponent)
- [Component-specific methods](#component-specific-methods)
- [CtrlrMidiMessage](#ctrlrmidimessage)
- [Sending MIDI](#sending-midi)
- [CtrlrMIDIDeviceManager & CtrlrMIDIDevice](#midi-devices)
- [CtrlrLuaUtils (`utils`)](#ctrlrluautils-utils)
- [CtrlrLuaMultiTimer (`timer`)](#ctrlrluamultitimer-timer)
- [Resources](#resources)
- [Callback hooks](#callback-hooks)
- [JUCE classes](#juce-classes)
- [Introspection](#introspection)

---

## Global objects

Every script can use these without creating them (assigned in
`CtrlrLuaManager::assignDefaultObjects`):

| Global | Type | What it is |
|---|---|---|
| `panel` | CtrlrPanel | The panel this script belongs to. Your main entry point. |
| `utils` | CtrlrLuaUtils | Dialogs, encoding, version, helpers. |
| `timer` | CtrlrLuaMultiTimer | Schedule repeating callbacks. |
| `devices` | CtrlrMIDIDeviceManager | Enumerate / open MIDI devices. |
| `resources` | CtrlrPanelResourceManager | Access embedded images/fonts/data. |
| `native` | CtrlrNative | OS-native helpers. |
| `afm` | AudioFormatManager | Audio file format support. |
| `atc` | AudioThumbnailCache | Waveform thumbnail cache. |
| `converter` | CtrlrLuaAudioConverter | Audio conversion helpers. |

## CtrlrPanel

The panel. Reach controls, send MIDI, manage state.

| Method | Purpose |
|---|---|
| `getModulatorByName(name)` / `getModulator(name)` | Get a modulator by its **Name** (case-sensitive). |
| `getModulatorByIndex(i)` | Get a modulator by index. |
| `getNumModulators()` | Count of modulators. |
| `getComponent()` | The panel's root component. |
| `getCanvas()` | The panel canvas (layers). |
| `getPanelEditor()` | The editor (size, canvas) — editing context. |
| `sendMidiMessageNow(msg)` | Send a `CtrlrMidiMessage` or a hex string immediately. |
| `sendMidi(msg, ts)` | Send with a timestamp (`CtrlrMidiMessage`, `MidiMessage`, or `MidiBuffer`). |
| `getGlobalVariable(i)` / `setGlobalVariable(i, v)` | Panel-wide integer variables. |
| `getProgramState()` / `setProgramState(...)` | Capture / restore all control values (a patch). |
| `getRestoreState()` / `setRestoreState(...)` / `isRestoring()` | Load-time state flags. |
| `getBootstrapState()` | Initial state. |
| `isLoading()` | True while the panel is loading. |

> 💡 Tip: `sendMidiMessageNow` accepts a plain hex string — `panel:sendMidiMessageNow("B0 4A 7F")` —
> as well as a `CtrlrMidiMessage` object.

## CtrlrModulator

One control's value/MIDI/identity. Get one with `panel:getModulatorByName(...)`.

**Values**

| Method | Purpose |
|---|---|
| `getValue()` / `getValueInt()` | Current raw value (number / int). |
| `setValue(v)` | Set the raw value. |
| `getModulatorValue()` / `getModulatorValueInt()` | Alias of `getValue` (seen in older panels). |
| `setModulatorValue(...)` | Alias of `setValue` (older panels). |
| `getValueMapped()` / `getValueMappedInt()` | Mapped value (see [Ch. 5](../05-value-mapping.md)). |
| `setValueMapped(...)` | Set by mapped value. |
| `getValueNonMapped()` / `getValueNonMappedInt()` | Explicit raw value. |
| `setValueNonMapped(...)` | Set explicit raw value. |

**Range**

| Method | Purpose |
|---|---|
| `getMinModulatorValue()` / `getMaxModulatorValue()` (+`…Int`) | Raw value range. |
| `getMinMapped()` / `getMaxMapped()` (+`…Int`) | Mapped range. |
| `getMinNonMapped()` / `getMaxNonMapped()` (+`…Int`) | Non-mapped range. |

**Identity / links**

| Method | Purpose |
|---|---|
| `getName()` / `getModulatorName()` | The modulator's name. |
| `getLuaName()` | Lua-safe name. |
| `getComponent()` | Its visual component. |
| `getMidiMessage()` | The `CtrlrMidiMessage` it's configured to send. |
| `getVstIndex()` | Host-automation index (see [Ch. 10](../10-distribution.md)). |
| `getRestoreState()` / `setRestoreState(...)` / `isRestoring()` | State flags. |

> ⚠️ Gotcha: `getModulatorByName` returns `nil` if the name doesn't match exactly — guard before
> chaining: `local m = panel:getModulatorByName("x"); if m then m:setValue(0) end`.

## CtrlrComponent

The visual side of a modulator. Get it via `modulator:getComponent()`.

| Group | Methods |
|---|---|
| **Geometry** | `getX()`, `getY()`, `getWidth()`, `getHeight()`, `getBounds()`, `setBounds()`, `setSize()`, `getRect()`, `getLuaBounds()` |
| **Value** | `getValue()`, `setValue()`, `getComponentValue()`, `setComponentValue()`, `getComponentMidiValue()`, `setComponentMidiValue()`, `getMidiValue()`, `setMidiValue()` |
| **Text** | `getComponentText()`, `setComponentText()`, `getTextForValue()` |
| **Range** | `getMinimum()`, `getMaximum()` |
| **Visibility / paint** | `isVisible()`, `setVisible()`, `repaint()` |
| **Mouse / keys** | `click()`, `mouseUp()`, `mouseEnter()`, `mouseExit()`, `isMouseButtonDown()`, `isMouseOver()`, `isMouseOverOrDragging()`, `keyPressed()` |
| **Transform / L&F** | `getTransform()`, `setTransform()`, `setCustomLookAndFeel()` |
| **Owner** | `getOwner()` (the owning modulator) |

## Component-specific methods

Beyond the shared `CtrlrComponent` API, some component types add their own (registered in
`CtrlrComponentLuaRegistration.cpp`):

| Component | Methods |
|---|---|
| **Toggle / Image button** | `getToggleState()`, `setToggleState()`, `isToggleButton()` (image), `getValueMap()` |
| **Label / LCD label** | `setText()`, `appendText()`, `getText()` |
| **Combo** | `getOwnedComboBox()`, `getSelectedId()`, `setSelectedId()`, `getSelectedItemIndex()`, `setSelectedItemIndex()`, `getText()`, `setText()`, `getValueMap()` |
| **Sliders** | `getOwnedSlider()` |
| **ListBox** | `getNumRows()`, `updateContent()`, `selectRow()`, `deselectRow()`, `deselectAllRows()`, `getNumSelectedRows()`, `getSelectedRow()`, `isRowSelected()`, `getSelectedRows()`, `setMultipleSelectionEnabled()` |
| **FileListBox** | `getNumSelectedFiles()`, `getSelectedFile()`, `setSelectedFile()`, `deselectAllFiles()`, `refresh()`, `scrollToTop()` |

## CtrlrMidiMessage

Construct, inspect, and modify MIDI messages.

**Construct** — `CtrlrMidiMessage(...)` accepts: a hex string (`"B0 4A 7F"`), a JUCE `MidiMessage`, a
`MemoryBlock`, or a Lua table of bytes.

| Method | Purpose |
|---|---|
| `getType()` / `setType(...)` | Status/type. |
| `getMidiMessageType()` / `setMidiMessageType(...)` | The `CtrlrMidiMessageType` (CC, NoteOn, SysEx, …). |
| `getChannel()` / `setChannel(c)` | MIDI channel. |
| `getNumber()` / `setNumber(n)` | CC/note/program number. |
| `getValue()` / `setValue(v)` | Value byte. |
| `getSize()` | Byte count. |
| `getData()` / `getLuaData()` | Raw bytes. |
| `getProperty(...)` / `setProperty(...)` | Generic property access. |
| `toString()` | Human-readable form. |
| `getInitializationResult()` | Whether construction succeeded. |

The `CtrlrMidiMessageType` constants (usable in Lua): `CC`, `Aftertouch`, `ChannelPressure`,
`NoteOn`, `NoteOff`, `SysEx`, `Multi`, `ProgramChange`, `PitchWheel`, `None`, `MidiClock`,
`MidiClockContinue`, `MidiClockStop`, `MidiClockStart`, `ActiveSense`.

## Sending MIDI

```lua
panel:sendMidiMessageNow("B0 4A 7F")                 -- hex string (CC 74 = 127, ch 1)
panel:sendMidiMessageNow(CtrlrMidiMessage("F0 7D 12 40 F7"))  -- SysEx
panel:sendMidi(CtrlrMidiMessage("C0 05"), 0)         -- with timestamp
```

> 🔗 Deeper: building Multi/NRPN/SysEx and receiving MIDI is in
> [Chapter 8](../08-sending-receiving.md).

## MIDI devices

`devices` is a `CtrlrMIDIDeviceManager`.

| Method | Purpose |
|---|---|
| `getNumDevices()` | Count of known devices. |
| `getDeviceName(i)` / `getDeviceByIndex(i)` | Device by index. |
| `getDeviceByName(name)` | Device by name. |
| `isDeviceOpened(...)` | Open state. |
| `getManagedDevices()` | All managed devices. |
| `refreshDevices()` | Rescan the system for devices. |

A `CtrlrMIDIDevice` offers: `openDevice()`, `closeDevice()`, `sendMidiMessage()`, `sendMidiBuffer()`,
`getName()`, `getState()`, `getType()`.

## CtrlrLuaUtils (`utils`)

| Group | Methods |
|---|---|
| **Dialogs** | `warnWindow()`, `infoWindow()`, `questionWindow()`, `askForTextInputWindow()`, `openFileWindow()`, `openMultipleFilesWindow()`, `saveFileWindow()`, `getDirectoryWindow()` |
| **Encoding / data** | `base64_encode()`, `base64_decode()`, `packDsiData()`, `unpackDsiData()`, `get8bitSigned()`, `get16bitSigned()` |
| **MIDI lists** | `getMidiInputDevices()`, `getMidiOutputDevices()` |
| **Version / misc** | `getVersionMajor()`, `getVersionMinor()`, `getVersionRevision()`, `getVersionString()`, `getPi()` |

## CtrlrLuaMultiTimer (`timer`)

Run a callback repeatedly.

| Method | Purpose |
|---|---|
| `setCallback(id, fn)` | Register a function for a timer slot. |
| `startTimer(id, ms)` | Start a timer (interval in ms). |
| `stopTimer(id)` | Stop it. |
| `isTimerRunning(id)` | Query. |
| `getTimerInterval(id)` | Current interval. |
| `isRegistered(id)` | Whether a callback is set. |

## Resources

`resources` is a `CtrlrPanelResourceManager`. Get an embedded resource and interpret it:

| Manager method | Purpose |
|---|---|
| `getResource(name)` | A `CtrlrPanelResource`. |
| `getNumResources()` / `getResourceIndex(name)` | Enumerate. |
| `getResourceAsImage(name)` / `getResourceAsFont(name)` | Typed access. |

A `CtrlrPanelResource` can be reinterpreted: `asImage()`, `asText()`, `asFont()`, `asXml()`,
`asData()`, `asAudioFormat()`, `getName()`, `getSize()`, `getFile()`, `createInputStream()`.

## Callback hooks

Attach a method to one of these via the modulator/panel/component callback dropdowns
([Chapter 7](../07-making-responsive.md)). Names come from
`Source/Resources/XML/CtrlrLuaMethodTemplates.xml`.

**Panel-level**

| Callback | Fires when |
|---|---|
| `luaPanelBeforeLoad` | Panel constructed (no modulators yet). |
| `luaPanelLoaded` | Panel fully loaded. |
| `luaPanelResourcesLoaded` | Resources finished loading. |
| `luaPanelSaved(saveType, file)` | Panel saved. |
| `luaPanelSaveState(tree)` / `luaPanelRestoreState(tree)` | Persist / restore custom state. |
| `luaCtrlrSaveState` / `luaCtrlrResoreState` | Ctrlr-level state. *(name spelled `Resore` in source.)* |
| `luaPanelResized` / `luaViewPortResized` | Layout changes. |
| `luaPanelPaintBackground(canvas, g)` | Custom background painting. |
| `luaPanelModulatorValueChanged(mod, value)` | Any modulator changed. |
| `luaPanelMidiReceived(midi)` | MIDI received. |
| `luaPanelMidiMultiReceived(multi)` | Multi-message received. |
| `luaPanelOSCReceived(path, types, args)` | OSC received. |
| `luaPanelProgramChanged(prog, bankLsb, bankMsb)` | Program change. |
| `luaPanelGlobalChanged(index, value)` | Global variable changed. |
| `luaPanelMidiChannelChanged` | MIDI channel changed. |
| `luaPanelMidiSnapshotPre` / `luaPanelMidiSnapshotPost` | Around a MIDI snapshot. |
| `luaPanelMessageHandler(message, type)` | Generic message handling. |
| `luaAudioProcessBlock(midiBuffer, posInfo)` | Audio block (plugin). |
| `luaPanelFileDragDropHandler` / `…DragEnterHandler` / `…DragExitHandler` | File drag & drop on the panel. |

**Modulator-level**

| Callback | Fires when |
|---|---|
| `luaModulatorValueChange(mod, value, source)` | This modulator's value changed. |
| `luaModulatorGetValueForMIDI(...)` | Convert value → MIDI (custom). |
| `luaModulatorGetValueFromMIDI(...)` | Convert MIDI → value (custom). |

The `source` argument (avoid feedback loops): `0` initial, `1` host, `2` MIDI in, `3` controller,
`4` GUI, `5` Lua, `6` program, `7` link, `8` unknown.

**Component-level**

| Group | Callbacks |
|---|---|
| **Mouse** | `uiCustomMouseDownCallback`, `…MouseUpCallback`, `…MouseDragCallback`, `…MouseMoveCallback`, `…MouseEnterCallback`, `…MouseExitCallback`, `…MouseDoubleClickCallback`, `…MouseWheelMoveCallback` |
| **Keys** | `uiCustomKeyDownCallback`, `uiCustomKeyStateChangedCallback` |
| **Paint / resize** | `uiCustomPaintCallback`, `uiCustomPaintOverChildrenCallback`, `uiCustomResizedCallback` |
| **Drag & drop** | `uiCustomIsInterestedInDragSourceCallback`, `uiCustomItemDragEnter/Move/ExitCallback`, `uiCustomItemDroppedCallback`, `uiCustomStartDraggingCallback` |
| **Per-type** | `uiTabsCurrentTabChanged`, `uiLabelChangedCbk`, `uiListBoxItemClicked/DoubleClicked/DeleteKeyPressed/ReturnKeyPressed`, `uiFileListFileClicked/DoubleClicked`, `uiWaveFormFilesDroppedCallback/SourceChangedCallback/ThumbnailChangedCallback` |
| **Legacy mouse** | `componentLuaMouseDown/Up/Drag/Moved/DoubleClick/Enter/Exit` |

## JUCE classes

CtrlrX exposes a large slice of [JUCE](https://docs.juce.com/master/index.html) to Lua. You construct
and use these like any object; consult the JUCE API docs for their methods. The most useful for panel
authors:

| Area | Classes |
|---|---|
| **Files & data** | `File`, `MemoryBlock`, `MemoryInputStream`, `ZipFile`, `GZIPDecompressorInputStream` |
| **Graphics** | `Graphics`, `Image`, `Colour`, `ColourGradient`, `Font`, `Path`, `AffineTransform`, `Rectangle`, `Point`, `Line`, `Justification`, `PathStrokeType` |
| **MIDI & audio** | `MidiMessage`, `MidiBuffer`, `AudioSampleBuffer`, `AudioFormatManager`, `AudioThumbnail`, audio format readers/writers |
| **Math / util** | `Random`, `Range`, `Time`, `String`, `StringArray`, `BigInteger`, `var`, `ValueTree`, `Expression` |
| **XML** | `XmlElement`, `XmlDocument` |
| **UI / input** | `Component`, `PopupMenu`, `AlertWindow`, `MouseEvent`, `KeyPress`, `ModifierKeys`, `LookAndFeelBase` |

> 💡 Tip: `Colour` is handy in paint callbacks — e.g. `g:setColour(Colour(0xFF303030))`.

## Introspection

The definitive, build-accurate reference is the running app itself:

```lua
how()                 -- every class bound to Lua
what(panel)           -- methods on the panel
what(some_modulator)  -- methods on a modulator
what(comp)            -- methods on a component
```

Use these whenever this page and your build seem to disagree — the build wins.

---

[← Lua Guide](01-lua-guide.md) | [Manual Index](../README.md)
