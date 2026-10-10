# Panel File Format

A `.panel` file is a JUCE `ValueTree` serialized to XML. The same tree also ships as `.panelz`
(gzipped XML), `.bpanelz` and `.bpanel`. A `.bpanelz` is **not** gzipped XML: it is a gzipped
*binary* `ValueTree::writeToStream` dump, which additionally carries the panel's **resources**
(images, fonts, data) base64-encoded in `<resource>` nodes.

`scripts/panel_tool.py` decodes all four formats, so you can `summary`, `lint` and `dump` a
`.bpanelz` directly rather than treating it as opaque. What you still cannot do is *write* one:
to ship a `.bpanelz` with resources, have the user export it from the CtrlrX app
(**File → Export**). Note the asymmetry — the app's XML export drops resource payload (across a
468-panel survey, 0 of 257 `.panel` files carried any, against 138 of 253 `.bpanelz`), so
`.bpanelz → .panel` is lossy for artwork even though it preserves everything structural.
Everything below describes the XML rendering of the tree, which is what you author and edit.

### The binary wire format

Should you ever need to decode it by hand (`panel_tool.py` already does):

```
tree  := string tag, compressedInt numProps, {string name, var value} * numProps,
         compressedInt numChildren, tree * numChildren
var   := compressedInt numBytes, byte marker, payload of numBytes-1 bytes
marker  1=int32  2=true  3=false  4=double  5=string  6=int64  7=array  8=binary  9=undefined
```

Strings are NUL-terminated UTF-8. A "compressed int" is one length byte — low 7 bits give how many
little-endian bytes follow, bit 7 means negative. See `ValueTree::readFromStream` and
`var::readFromStream` in JUCE, and `CtrlrPanel::openBinPanel` in the CtrlrX source.

## Top-level shape

```xml
<?xml version="1.0" encoding="UTF-8"?>
<panel name="My Panel" ...many panel attributes...>
  <uiWindowManager/>
  <luaManager>
    <luaManagerMethods>
      <luaMethodGroup name="Built-In" uuid="…">
        <luaMethod .../>
        ...
      </luaMethodGroup>
    </luaManagerMethods>
  </luaManager>
  <uiPanelEditor uiPanelCanvasRectangle="0 0 600 400" name="Test Panel" uiPanelEditMode="1" .../>
  <modulator ...>
    <midi .../>
    <component ... uiType="uiSlider"/>
  </modulator>
  <modulator ...> ... </modulator>
  ...
</panel>
```

Order matters to readers expecting it: `<uiWindowManager/>` and `<luaManager>` come first, then
`<uiPanelEditor>`, then the `<modulator>` elements. Keep that order when generating a panel.

## The `<panel>` root attributes

There are ~80 of them. You rarely set more than a handful; copy the rest from a template. The ones
worth knowing:

| Attribute | Meaning |
|---|---|
| `name` | Panel name (tab title, plugin name). |
| `panelAuthorName`, `panelVersionMajor`/`Minor`/`Name` | Metadata. |
| `panelVendor`, `panelDevice` | Shown when the panel runs as a plugin. |
| `panelPlugType` | e.g. `Instrument\|Synth`. |
| `panelMidiInputDevice`, `panelMidiOutputDevice`, `panelMidiControllerDevice` | Selected MIDI ports (device-specific strings; in fixtures these are mock names like `"Device 00, port 00"`). |
| `panelMidiInputChannelDevice`, `…OutputChannelDevice` | Default channels. |
| `panelMidiInputFromHost`, `panelMidiOutputToHost`, `panelMidiThruH2H`/`H2D`/`D2H`/`D2D` | MIDI routing flags (host↔device thru). |
| `luaPanel…` (many) | **Panel-level Lua callbacks**, each holds a method *name* or `"-- None"`. See `lua-api.md`. |
| `panelGlobalVariables` | 64 colon-separated ints — the panel's global variable slots (`-1` = unset). |
| `panelUID`, `panelInstanceUID`, `panelInstanceManufacturerID` | Identity; keep unique per panel. |

> When you build a fixture, the simplest path is to copy an existing fixture's `<panel …>` line
> wholesale and change only `name`, version/vendor/device, and any `luaPanel…` callbacks you need.

## The `<modulator>` element

The wrapper for one control. Key attributes (full list is large; these carry meaning):

| Attribute | Meaning |
|---|---|
| `name` | Identity. Case-sensitive, unique. Lua looks it up by this. |
| `modulatorMin`, `modulatorMax` | Raw value range (floats, e.g. `0.0`/`127.0`). |
| `modulatorValue` | Current raw value. |
| `modulatorIsStatic` | `1` for display-only elements (labels/images/groups) that hold no value. |
| `vstIndex` | Host-automation parameter index. **Unique per panel**; `-1`/absent = not exported. |
| `modulatorVstExported` | Whether this modulator is exposed to the host. |
| `modulatorValueExpression` | value → MIDI math (default `modulatorValue`). |
| `modulatorValueExpressionReverse` | MIDI → value math (default `midiValue`). |
| `modulatorControllerExpression` | MIDI-learn input → value (default `value`). |
| `luaModulatorValueChange` | Method name called when the value changes (or `"-- None"`). |
| `luaModulatorGetValueForMIDI` | Method name: custom value→MIDI conversion. |
| `luaModulatorGetValueFromMIDI` | Method name: custom MIDI→value conversion. |
| `modulatorGlobalVariable` | Index of a panel global to mirror, or `-1`. |
| `modulatorExcludeFromSnapshot`, `modulatorMute`, `modulatorMuteOnStart` | Behavior flags. |
| `modulatorLinkedTo…` | Link this modulator's value to another modulator/property. |

### The `<midi>` child

```xml
<midi midiMessageType="0" midiMessageChannelOverride="0" midiMessageChannel="1.0"
      midiMessageCtrlrNumber="74.0" midiMessageCtrlrValue="0.0"
      midiMessageMultiList="" midiMessageSysExFormula=""/>
```

| Attribute | Meaning |
|---|---|
| `midiMessageType` | **Integer enum** — `0=CC, 1=Aftertouch, 2=ChannelPressure, 3=NoteOn, 4=NoteOff, 5=SysEx, 6=Multi, 7=ProgramChange, 8=PitchWheel, 9=None, 10=MidiClock, 11=MidiClockContinue, 12=MidiClockStop, 13=MidiClockStart, 14=ActiveSense`. |
| `midiMessageChannel` | 1–16 (stored as a float like `1.0`). |
| `midiMessageChannelOverride` | `1` to use this channel instead of the panel default. |
| `midiMessageCtrlrNumber` | CC/note/program number. |
| `midiMessageCtrlrValue` | **Ignored for ordinary messages** — CC/Note/Aftertouch/ChannelPressure/PitchWheel take their value byte from the modulator's current value, not from here. See `midi-and-values.md`. |
| `midiMessageMultiList` | Sequence string for `Multi`. See `midi-and-values.md`. |
| `midiMessageSysExFormula` | Token template for `SysEx`. See `midi-and-values.md`. |

### The `<component>` child

Holds the visual. **Many** attributes (every colour, the label font, effect offsets, mouse-callback
names, plus a big block of type-specific look properties). The ones you actually set:

| Attribute | Meaning |
|---|---|
| `uiType` | The component kind — `uiSlider`, `uiCombo`, `uiToggleButton`, `uiLabel`, … (see `components.md`). |
| `componentRectangle` | `"x y w h"` — position and size on the canvas, in px. |
| `componentVisibleName` | The label text. |
| `componentLabelVisible` | `1`/`0` — show the name label. |
| `componentLabelPosition` | `top`/`bottom`/`left`/`right`. |
| `componentValueDecimalPlaces` | Decimals shown in a value readout. |
| `componentGroupName`, `componentGroupped` | Group membership. |
| `componentLayerUid` | Which canvas layer it's on — a hex UID matching a `<uiPanelCanvasLayer uiPanelCanvasLayerUid>` in `<uiPanelEditor>`. Layers stack by `uiPanelCanvasLayerIndex`, **higher on top** (`CtrlrPanelCanvas::compareElements`); every layer is full-canvas but, outside edit mode, passes clicks through to the layers below (`setInterceptsMouseClicks(false, true)`), so a layer used as a page needs its own opaque group. A layer saved with `uiPanelCanvasLayerVisibility="0"` starts hidden; `panel:getCanvas():getLayerByName(n):setVisible(b)` switches it at runtime. |
| `componentLua…` (Mouse*, etc.) | Legacy per-component mouse-callback method names. |
| type-specific, e.g. `uiSliderStyle`, `uiComboContent`, `uiImageButtonResource` | Look/behavior for that `uiType`. |

> The `uiSliderStyle` (and similar) values are strings: `LinearHorizontal`, `LinearVertical`,
> `LinearBar`, `Rotary`, `RotaryHorizontalDrag`, `RotaryVerticalDrag`,
> `RotaryHorizontalVerticalDrag`, `IncDecButtons`, `TwoValueHorizontal/Vertical`,
> `ThreeValueHorizontal/Vertical`.

## Value maps (named choices in XML)

A value map turns raw values into named choices (for `uiCombo`, `uiFixedSlider`,
`uiFixedImageSlider`). It is stored as child data of the modulator/component as an ordered set of
entries, each pairing a *non-mapped* (raw) value with a *mapped* value and display *text*. The
component shows the text; the processor sends the raw value. When authoring, the most reliable
approach is to copy a value-map block from a panel that already has a working combo and edit the
entries, rather than synthesizing the structure — the exact child element names vary by build, so
confirm against a real example in the repo (search panels for `uiCombo` + a values list).

## The `<luaManager>` block

```xml
<luaManager>
  <luaManagerMethods>
    <luaMethodGroup name="Built-In" uuid="49c3f79ca3dc4f868d5cfc5e48e56199">
      <luaMethod luaMethodName="doSend"
                 luaMethodCode="doSend = function(mod, value)&#10;&#9;panel:sendMidiMessageNow(&quot;B0 0E 42&quot;)&#10;end"
                 luaMethodLinkedProperty="" luaMethodSource="0"
                 uuid="00000000000000000000000000000013" luaMethodValid="1"/>
    </luaMethodGroup>
    <!-- a <luaMethod> may also sit directly under <luaManagerMethods>, with no group: -->
    <luaMethod luaMethodName="helper" luaMethodCode="helper = function() end"
               luaMethodLinkedProperty="" luaMethodSource="0" uuid="…" luaMethodValid="1"/>
  </luaManagerMethods>
</luaManager>
```

> `<luaMethod>` elements can be organized into one or more `<luaMethodGroup>`s **or** placed directly
> inside `<luaManagerMethods>` with no group — both are valid, and the two can mix. What matters is
> that the method exists *somewhere* under `<luaManagerMethods>`; the group is only for organization.
> When adding a method you can drop it into an existing group or alongside the others ungrouped.
> (`panel_tool.py` finds methods either way.)

| Attribute | Meaning |
|---|---|
| `luaMethodName` | The method's name — referenced by callback attributes elsewhere. |
| `luaMethodCode` | The Lua source, **XML-escaped**, with the inner function named to match `luaMethodName`. |
| `luaMethodLinkedProperty` | Usually empty; set when the method is bound to a specific property template. |
| `luaMethodSource` | `0` = the code is in `luaMethodCode` (normal). `1` = the code lives in an **external `.lua` file** named by `luaMethodSourcePath`; see below. |
| `luaMethodSourcePath` | Only with `luaMethodSource="1"`: the file holding the code, relative to the panel's lua dir (or absolute). `luaMethodCode` is then absent. |
| `uuid` | Unique id for the method. |
| `luaMethodValid` | `1`. |

### Lua in external files — `luaMethodSource="1"`

A method's code does **not** have to live inside the XML. Set `luaMethodSource="1"`, add
`luaMethodSourcePath`, and drop `luaMethodCode` entirely:

```xml
<luaMethod luaMethodName="codec" luaMethodSourcePath="codec.lua"
           luaMethodLinkedProperty="" luaMethodSource="1" uuid="…" luaMethodValid="1"/>
```

This is worth knowing for any panel whose Lua is large or version-controlled: the `.lua` files get
real syntax highlighting, diffs, linting and `luac -p`, and they can be run under a plain `lua5.1`
interpreter in a test harness — while the *shipped* panel stays self-contained (see export, below).

**Where the file is looked up.** `<panelFilePath minus its extension>/lua/<luaMethodSourcePath>`,
i.e. for `…/MyPanel.panel` the dir is `…/MyPanel/lua/`. An absolute `luaMethodSourcePath` is honoured
as-is (`CtrlrPanel::getLuaMethodSourceFile`). The path is **relative to that lua dir, not to the
panel file**.

**A stale `panelFilePath` in the XML does not break it.** `CtrlrManager::openPanelInternal(const
File&)` overwrites `panelFilePath` with the file actually being opened before the panel is added, and
`CtrlrPanel::restoreState` sets that property immediately *before* restoring the `<luaManager>` —
there is a comment in the source saying it does so precisely to make these relative paths resolve.
So a panel with someone else's absolute path baked in still finds its own lua dir, including under
`CtrlrPanelProbe` (which opens via the same CLI path).

**Export inlines the files automatically.** `getPanelExportTree` calls
`convertLuaMethodsToPropeties(getPanelLuaDir(), exportTree)`, so an exported `.bpanelz`/`.panelz`
carries the code back in `luaMethodCode` and needs no lua dir. The GUI also goes the other way —
`convertLuaMethodsToFiles` explodes an existing panel's Lua into that dir, **one file per method,
named after the method**, in subdirectories mirroring `<luaMethodGroup>` nesting.

> **Hazard: a missing file is not reported at load.** Measured with the probe — with the lua dir
> renamed away, the panel loaded with `luaCompileErrors` **empty** and no load error; the method's
> globals were simply `nil`, and the failure only surfaced when something called them. So a `.panel`
> copied without its lua dir looks fine and breaks at first use. Distribute the **exported** panel,
> and have your tooling check that every `luaMethodSource="1"` path resolves (`panel_tool.py lint`
> does).

### Method bodies run at load, in document order

Each `<luaMethod>`'s code is executed as a Lua **chunk** when the panel is restored
(`CtrlrLuaMethod::setCodeInternal` → `runCode`), and `CtrlrLuaMethodManager::restoreState` walks the
methods in document order. Two consequences:

- **Top-level statements in a method body run at panel load**, so a `<luaMethod>` is a perfectly good
  place to define shared constants or several helper functions — it need not define a function of its
  own name at all. (Many real panels rely on this.)
- **Order between methods matters** for top-level code: a method whose body *reads* a global at the
  top level must appear after the one that sets it. Reading it inside a function body instead makes
  the ordering irrelevant — prefer that.

### XML escaping for `luaMethodCode`

The code is a single attribute value, so escape it:

| Character | Escape |
|---|---|
| newline | `&#10;` |
| tab | `&#9;` |
| `"` | `&quot;` |
| `&` | `&amp;` |
| `<` | `&lt;` |
| `>` | `&gt;` |

Get the escaping wrong and the file fails XML parsing (or silently truncates the method). Always run
the well-formedness check after writing Lua into a panel.

## Minimal authoring checklist

- [ ] `<panel>` root copied from a template; `name`/version/device set.
- [ ] `<uiWindowManager/>`, `<luaManager>`, `<uiPanelEditor>` present and in order.
- [ ] Each `<modulator>` has a unique `name` and (if exported) unique `vstIndex`.
- [ ] Each modulator has a `<midi>` and a `<component uiType=…>`.
- [ ] Any Lua method referenced by a callback attribute exists in `<luaManager>` with a matching
      inner function name, properly escaped.
- [ ] File passes an XML well-formedness check and loads in the test harness / app.
