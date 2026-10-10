---
name: ctrlrx-panel-builder
description: >-
  Author and edit CtrlrX panel files (.panel / .bpanelz) and their Lua by hand — building or
  modifying the XML ValueTree directly rather than clicking around the CtrlrX GUI. Use this
  whenever the task involves a CtrlrX panel: adding or changing a modulator (knob/slider/button/
  combo/label), wiring a control to MIDI (CC, Note, Program Change, Pitch Bend, NRPN/RPN, 14-bit,
  or SysEx), writing or attaching a Lua method/callback, building a value map or value expression,
  creating a test fixture panel for the GoogleTest harness, or debugging why a hand-edited panel
  won't load or behave. Trigger it even when the user just says ".panel file", "modulator",
  "uiSlider/uiCombo/uiToggleButton", "SysEx formula", "Multi message list", "panel Lua", or names
  a synth editor they want to build for CtrlrX — they almost always need this format knowledge.
---

# Building CtrlrX Panels (by editing the file)

A CtrlrX **panel** is a JUCE `ValueTree`. The same tree is saved in four forms: `.panel` (plain
XML), `.panelz` (gzipped XML), `.bpanelz` (a gzipped *binary* ValueTree that also embeds the
panel's resources — images, fonts, data) and `.bpanel` (the same binary, uncompressed).
Everything structural about a panel lives in that tree: the controls, their MIDI mappings, the value
maps, and the Lua source.

`scripts/panel_tool.py` **reads all four** and presents them identically, so you can inspect a
`.bpanelz` directly instead of asking the user to export one. You still **author in the `.panel`
(XML) form**, which is what you do when you generate panels, build test fixtures, or fix a panel
programmatically. Producing a `.bpanelz` is still the user's job via the CtrlrX app's export —
the XML export path drops embedded resources, so a `.panel` round-trip loses the panel's artwork.
(The end-user GUI building workflow is documented for humans in `Doc/manual/` of the CtrlrX
repo; you rarely need it here.)

## Use the bundled tool — don't read the raw XML

A real `.panel` is tens of thousands of tokens of verbose XML, and a `.bpanelz` cannot be read at
all without decoding the binary ValueTree. A bundled script, `scripts/panel_tool.py` (stdlib
Python 3, no deps), does the token-heavy and error-prone parts for you. **`FILE` below may be any
of `.panel`, `.panelz`, `.bpanelz` or `.bpanel`** — the tool sniffs the format and every subcommand
behaves the same way on all of them:

```bash
python3 scripts/panel_tool.py summary FILE          # compact overview: each modulator's name,
                                                   # uiType, MIDI mapping, value range, attached Lua
python3 scripts/panel_tool.py lint    FILE          # XML well-formedness + structural checks:
                                                   # unique names, dangling Lua refs, missing uiType,
                                                   # duplicate vstIndex, plus MIDI wiring traps that
                                                   # load fine but emit wrong bytes (one-line
                                                   # uiButtonContent, 7-bit PitchWheel, ignored
                                                   # midiMessageCtrlrValue, slider range mismatch).
                                                   # Exit 0 = clean.
python3 scripts/panel_tool.py escape  lua.txt      # XML-escape raw Lua for a luaMethodCode attribute
echo 'f = function() end' | python3 scripts/panel_tool.py escape   # (or from stdin)
python3 scripts/panel_tool.py dump FILE modulator:NAME   # one element's XML as a copy-template
                                                   # (also: 'dump FILE editor|panel|modulators')
                                                   # dump output is XML-escaped, so an element
                                                   # lifted out of a .bpanelz pastes straight
                                                   # into a .panel
```

Reach for `summary` first to understand an existing panel, `dump` to lift a known-good element
(a modulator, or the `<uiPanelEditor>` with its canvas-layer defs) as a copy-template instead of
reading the huge raw XML, `escape` whenever you put Lua into the XML, and `lint` after every edit
before you claim it's done.

## Ground truth: the CtrlrX source tree

The reference files bundled here were distilled from a specific CtrlrX version. Formats evolve and
builds differ, so **when a detail is missing, ambiguous, or your panel behaves unexpectedly, consult
the user's CtrlrX source — it is the final authority.** Assume the user has the repository checked
out; if you don't already know where it lives, **ask** ("Where's your CtrlrX source checkout?")
rather than guessing a path. Once you know it, these are the files that answer most questions:

| Question | Look in |
|---|---|
| Exact `midiMessageType` (and other) enum integer values | `Source/MIDI/CtrlrMidiMessage.h` |
| Property / attribute names used in the XML | `Source/Core/CtrlrIDs.h`, `CtrlrIDManager` |
| What component types exist and their properties/defaults | `Source/UIComponents/CtrlrComponents/` |
| Which methods/classes Lua actually exposes (the real API) | `Source/Lua/CtrlrLuaManager.cpp`, `Source/Lua/CtrlrComponentLuaRegistration.cpp`, `Source/MIDI/CtrlrMidiMessage.cpp` |
| SysEx tokens and checksum kinds | `Source/Core/CtrlrSysexProcessor.*`, `Source/Core/CtrlrSysexToken.*` |
| Callback hook names + their pre-filled signatures | `Source/Resources/XML/CtrlrLuaMethodTemplates.xml` |
| Real, guaranteed-loadable panel examples to copy | `Tests/fixture_*.panel`, `Panels/` |
| The human-facing manual (deeper GUI/Lua walkthroughs) | `Doc/manual/` |

For C/C++ navigation in the source, prefer clangd (goToDefinition / findReferences / workspaceSymbol)
over grep when it's available — it's faster and more accurate for enums, bindings, and call sites.

## The one model to hold in your head

Everything you add to a panel is a **modulator**. A modulator is three nested pieces:

```
<modulator ...value, range, expressions, Lua-callback names...>   ← identity + behavior
  <midi ...message type, channel, number, value, multi/sysex... /> ← what MIDI it sends/matches
  <component ...look, position, size... uiType="uiSlider"/>        ← the visual; uiType picks the kind
</modulator>
```

- The **modulator** carries the value, the min/max range, the value-conversion expressions, and the
  *names* of any Lua methods attached to it.
- The **`<midi>`** child is the MIDI message it emits (and the pattern it matches on the way in).
- The **`<component>`** child is the on-screen widget; its `uiType` attribute (e.g. `uiSlider`,
  `uiCombo`, `uiToggleButton`, `uiLabel`) decides which kind of control it is.

Lua is stored **separately** in a `<luaManager>` block near the top of the panel, and modulators
*reference methods by name*. Keep that split in mind — it's the thing that surprises people.

## The golden rule: copy a working modulator, then change it — don't hand-write from scratch

A single `<modulator>` carries **dozens** of attributes (label font, every colour, effect offsets,
VST export flags, link properties…), most with non-obvious defaults. CtrlrX is forgiving about some
missing attributes and silently wrong about others — a panel with a malformed or partial modulator
may fail to load, load with the control invisible, or load with surprising behavior. You will waste
far more time inventing attributes than copying them.

So the reliable workflow is **duplicate-and-edit**:

1. Find a known-good modulator of the right `uiType` — in the CtrlrX repo, `Tests/fixture_*.panel`
   are compact, guaranteed-loadable templates, and `assets/template-panel.panel` (bundled here) is a
   minimal one-slider panel. Pick the closest match.
2. Copy that whole `<modulator>…</modulator>` block.
3. Change only the handful of attributes that define *your* control (see below).
4. Give it a unique `name`, and a unique `vstIndex` / `componentLayerUid` if the format needs it.

Only the small set of attributes in the next sections actually encodes intent; treat the rest as
boilerplate you carry along.

## The attributes that actually matter

When you copy a modulator, these are the ones you deliberately set. Everything else is look-and-feel
boilerplate.

**On `<modulator>`:**
- `name` — the control's identity. **Case-sensitive**, must be unique, this is what Lua's
  `panel:getModulatorByName("…")` looks up.
- `modulatorMin`, `modulatorMax`, `modulatorValue` — raw value range and current value (e.g.
  `0`/`127` for a 7-bit CC, `0`/`16383` for 14-bit, `0`/`1` for a switch).
- `modulatorValueExpression` / `modulatorValueExpressionReverse` — math applied value↔MIDI
  (default pass-through `modulatorValue` / `midiValue`). See `references/midi-and-values.md`.
- `luaModulatorValueChange`, `luaModulatorGetValueForMIDI`, `luaModulatorGetValueFromMIDI` — the
  **name** of a Lua method to call, or `"-- None"` for none. The method must exist in `<luaManager>`.

**On `<midi>`:**
- `midiMessageType` — an **integer** enum (this trips people up):
  `0=CC, 1=Aftertouch, 2=ChannelPressure, 3=NoteOn, 4=NoteOff, 5=SysEx, 6=Multi, 7=ProgramChange,
  8=PitchWheel, 9=None`, then `10=MidiClock, 11=MidiClockContinue, 12=MidiClockStop,
  13=MidiClockStart, 14=ActiveSense`.
- `midiMessageChannel` — 1–16.
- `midiMessageCtrlrNumber` — CC number / note number / program number.
- `midiMessageCtrlrValue` — **not** the value byte of an ordinary message, despite the name. For
  CC / NoteOn / NoteOff / Aftertouch / ChannelPressure / PitchWheel, CtrlrX builds the message from
  the **modulator's current value** (`getValue()`) and never consults this attribute
  (`CtrlrMidiMessage.cpp:1004-1019`). You therefore *cannot* give a trigger button a fixed velocity
  by setting `midiMessageCtrlrValue="100"` — a `0..1` button still emits velocity 0 or 1. To send a
  fixed value byte, set `midiMessageType="9"` and emit the message from Lua.
- `midiMessageMultiList` — the sequence string for `Multi` type.
- `midiMessageSysExFormula` — the byte-template string for `SysEx` type.

**On `<component>`:**
- `uiType` — which widget (`uiSlider`, `uiCombo`, `uiToggleButton`, `uiLabel`, …). See
  `references/components.md`.
- `componentRectangle` — `"x y w h"` position and size on the canvas.
- `componentVisibleName` — the label text shown to the user.
- type-specific look (e.g. `uiSliderStyle`) — copy/adjust from a same-`uiType` template.

## Lua: stored once, referenced by name

Lua methods do **not** live on the modulator. They live in the panel's `<luaManager>` block:

```xml
<luaManager>
  <luaManagerMethods>
    <luaMethodGroup name="Built-In" uuid="…">
      <luaMethod luaMethodName="cutoffChanged"
                 luaMethodCode="cutoffChanged = function(mod, value)&#10;&#9;console(value)&#10;end"
                 luaMethodLinkedProperty="" luaMethodSource="0" uuid="…" luaMethodValid="1"/>
    </luaMethodGroup>
  </luaManagerMethods>
</luaManager>
```

Methods can be grouped under one or more `<luaMethodGroup>` elements, **or** placed directly inside
`<luaManagerMethods>` with no group at all — both are valid and can coexist. The group is just
organization; what matters is that the method exists somewhere under `<luaManagerMethods>`. When you
add one, dropping it into an existing group or alongside the others ungrouped both work.

Two rules that bite if you forget them:

1. **The code is an XML attribute, so it must be XML-escaped.** Newline → `&#10;`, tab → `&#9;`,
   `"` → `&quot;`, `&` → `&amp;`, `<` → `&lt;`, `>` → `&gt;`. The body is one long escaped string.
   Don't escape by hand — write the Lua plainly and run it through
   `python3 scripts/panel_tool.py escape` to get the attribute-ready string.
2. **The inner `function` name must match `luaMethodName`.** The code reads
   `cutoffChanged = function(...)`, and the modulator references it as
   `luaModulatorValueChange="cutoffChanged"`. All three names are the same string. A callback
   attribute set to a method name that doesn't exist in `<luaManager>` simply never fires.

To attach a method to a control: add the `<luaMethod>` to `<luaManager>`, then set the right
callback attribute on the modulator (or `<component>`, or the `<panel>` root) to that method's name.

CtrlrX embeds **Lua 5.1** (1-based arrays, `..` for concat). Inside any method you get ready-made
globals: `panel`, `utils`, `timer`, `devices`, `resources`. The callback's parameter list is fixed
per callback type — don't change it. Full API and the callback catalogue: `references/lua-api.md`.

## Typical workflow for an edit

1. **Read the target panel** and locate the `<modulator>`s, the `<luaManager>`, and the `<panel>`
   root attributes — use `panel_tool.py summary`/`dump`, which handle `.bpanelz` and `.panelz` as
   readily as `.panel`. (A `.bpanelz` is *not* gzipped XML: it is a gzipped binary ValueTree, so
   gunzip alone gets you nothing readable.)
2. **Plan the change** in terms of the model above: new modulator? change a MIDI mapping? add Lua?
3. **Copy a template** modulator/method of the right kind and edit only the meaningful attributes.
4. **Keep names and IDs unique** (`name`, and `vstIndex`/`uuid`/`componentLayerUid` where present).
5. **Verify it loads** before claiming done (next section). A panel that won't load is the #1
   failure mode of hand-editing.

## Verify — never assume a hand-edited panel is valid

Two cheap checks, in order:

1. **Lint** — `python3 scripts/panel_tool.py lint your.panel`. This catches the common hand-edit
   failures: malformed XML (usually mis-escaped Lua), a callback pointing at a Lua method that
   doesn't exist, a missing `uiType`, duplicate modulator names, or a duplicate `vstIndex`. It also
   warns about MIDI wiring that loads happily but emits the wrong bytes — a one-line
   `uiButtonContent`, a PitchWheel left at 7-bit range, a `midiMessageCtrlrValue` that will be
   ignored, a slider whose component range disagrees with its modulator range. Exit 0 means no
   *errors*; read the warnings, they are usually real. 
2. **It actually loads in CtrlrX.** In the CtrlrX repo the GoogleTest harness is the fastest loader:
   the `ProcessorInstance` fixture loads a `.panel` and you can assert on it (e.g. modulator count,
   that a control exists, that moving it emits the expected MIDI). See `Tests/test_ProcessorPanel.cpp`
   and `Tests/test_ProcessorFixture.h`. Build/run with `./test.sh && ./build/Tests/TestsProcessor`.
   Outside the repo, load the panel in the Standalone app and confirm the controls appear and respond.

Report what you actually verified — "XML is well-formed and it loads with N modulators" beats "looks
right".

### Verifying *behavior* (not just loading) — `CtrlrPanelProbe`

Lint + load prove the panel is well-formed, not that it *works*. Dead button wiring, a slider that
clamps silently, a `luaModulatorValueChange` that never fires, a SysEx formula that emits the wrong
bytes — all pass lint and load fine. To catch those without hardware, use the headless probe (in
the CtrlrX repo, on the `headless-panel-runner`/`basic_test_coverage` branch line):

```bash
cmake --build build --target CtrlrPanelProbe -j 12

cat > check.json <<'EOF'
{ "settleMs": 300, "steps": [
  { "action": "setValue", "modulator": "cutoff", "value": 100 },
  { "action": "click",    "modulator": "fetchBtn" }
] }
EOF
./build/Tests/CtrlrPanelProbe your.panel check.json
```

It loads the panel, rebinds its MIDI to mock devices, runs the action script, and prints a JSON
report: per-step `deviceMidiOut` / `hostMidiOut` (hex bytes actually emitted), `errors`, and a
`finalState` snapshot of every modulator. Actions: `setValue` (drives the real GUI wiring, so MIDI +
value-change Lua fire), `click`, `injectDeviceMidi`, `injectHostMidi`, `callLuaMethod`, `eval`,
`waitFor`, `screenshot`, `setLayerVisible`, `wait`. Exit `0` clean / `1` an error occurred / `2` load
or script failure. It **reports observations; you assert** — e.g. "step 0 `deviceMidiOut` should
contain `b0 4a 64`". Empty `deviceMidiOut` after a `setValue` means the control's MIDI wiring is
dead.

**`setValue` bypasses the component's own range**, so emitted bytes alone cannot prove a range is
right: a `uiSlider` limited to 0..127 driving a PitchWheel still emits `ef 7f 7f` when the script
sets 16383, even though a user could never turn the knob past 1/128 of the bend. Assert the declared
range separately — `eval` back `modulatorMax` and `uiSliderMax` — whenever the range is the thing
under test. Full schema + worked example: **`Tests/PanelProbe.md`** in the repo — read it, the notes here
are only the highlights.

**A `uiCombo` sends its MIDI *twice* under `setValue`** — expect the doubled stream, do not report it
as a panel bug. `CtrlrCombo::setComponentValue` (`CtrlrCombo.cpp`) calls
`ctrlrCombo->setSelectedId(v + 1, sendNotificationSync)` and *then* `setValueGeneric(…changedByGUI)`.
Entered from the API with a **changed** id, `setSelectedId` synchronously re-enters `comboBoxChanged`,
which calls `setComponentValue` and sends; the outer call then sends again. A real user click enters at
`comboBoxChanged` with the id already current, so JUCE fires nothing and only one send happens. The two
identical `MidiBuffer`s merge by sample index, so the bytes arrive *interleaved*
(`b0 63 00  b0 63 00  b0 62 01  b0 62 01  …`), not as two clean bursts. Sliders and buttons emit once.
Collapse runs of byte-identical consecutive messages before asserting; that undoes exactly this and
leaves a genuine double-fire (two *different* values) visible.

**Assert on `summary.errorCount`.** Reports carry `"reportVersion": 1`. Every failure — an action the
probe could not perform, a Lua method that threw, a method that failed to compile — lands in one
array, `steps[i].errors = [{type: "action"|"lua", message}]`, and `summary.errorCount` totals all of
them. (Older probe versions split these across `step.error` / `step.luaErrors` with a `summary.luaErrorCount`
that excluded step errors, so a test checking one number passed vacuously while the method it called
threw on every call. If you meet a report with no `reportVersion`, that is the old shape.)

`callLuaMethod` reports the method's return value as `steps[i].returnValue` **with or without `args`**
— no dummy argument needed — and `args` may contain nested arrays/objects, which arrive as Lua tables.

**Reading panel internals: use `eval`, not an injected scratch method.**
`{ "action": "eval", "lua": "return codec.get(img, 'filters.speed')" }` runs the chunk in the panel's
own `lua_State`, so module tables and globals are directly readable; the result converts to JSON
(Lua tables → arrays when keyed `1..n`, else objects). The chunk is tried as an expression first, so
a bare call yields its value. This retires the old "inject a throwaway `<luaMethod>` into a scratch
copy of the panel" trick for probing bindings — do it with `eval` instead.

**`settleMs` is a ceiling, not a floor.** Settling stops as soon as the panel has been quiet
(no MIDI, no log line) for `quietMs` (default 30). Both are settable per action as well as
script-wide. Use `wait` (a true floor) or `waitFor` (polls a Lua condition) for async flows.

**Verifying a `uiCustomComponent`'s paint headlessly.** The panel's component tree *is* built and
laid out under the probe (there is no window, but `getCanvas()` is not null), so the simplest check
is `{ "action": "screenshot", "file": "out.png" }` — it renders the canvas via
`createComponentSnapshot` and writes a real PNG you can look at.

**`repaint()` is inert under the probe, and it reports no error.** With a counter inside the paint
callback, `comp:repaint()` followed by a 600 ms wait leaves the count at **0**; each `screenshot`
raises it by exactly **1**. `repaint()` only marks the component dirty for a message loop that never
runs headlessly, while `createComponentSnapshot` paints synchronously. The step returns normally and
`summary.errorCount` stays 0 — so a test that drives state, calls `repaint()` and then asserts on a
paint side effect **passes vacuously, for ever**. Paint via `screenshot` or the offscreen `Graphics`
recipe below, never via `repaint()`.

**`click` cannot drive a drawing surface.** It reaches a `uiCustomComponent`'s
`uiCustomMouseDownCallback`, but the probe builds *every* click at the component's dead centre
(`makeClickEvent`, `probe_Main.cpp`) and has **no drag action at all** — measured on a 1400x530
component: one `click` delivered 1 mouseDown at exactly `(w/2, h/2)` and 0 drags. So `click` proves
the wiring is live and nothing more. **Put a drawing surface's hit-testing and edit logic in pure
functions over plain numbers**, with the `uiCustom*` callbacks as thin wrappers that read `e.x`/`e.y`
and call them; then drive the core with coordinates through `eval`/`callLuaMethod` (and, being pure,
under a bare `lua5.1` too).

To assert on specific pixels instead, drive the callback against real JUCE bindings with `eval`,
building a `Graphics` over an offscreen image:

```lua
local comp = panel:getModulatorByName("my_canvas"):getComponent()
local img  = Image(Image.ARGB, comp:getWidth(), comp:getHeight(), true)
local g    = Graphics(img)                      -- Graphics has a constructor<Image&>
myPaintCallback(comp, g)                        -- the shipped callback, unmodified
return string.format("%08x", img:getPixelAt(x, y):getARGB())
```

This catches a wrong `Graphics` overload or a nil glyph table, which lint and a plain load both miss.

**Overlapping components.** A custom component paints over whatever it overlaps and clips it
silently. `finalState.overlaps` now reports intersecting pairs of effectively-visible controls, so
you no longer need to hand-roll a clearance check in a generator — but note it uses
`canvasRectangle` (absolute), *not* `componentRectangle`, which is `getBoundsInParent()` and
therefore group-relative for a grouped control.

**Visibility has three fields**, and the one you usually want is `effectiveVisible` (the component
and every ancestor up to the canvas). `visible` is only the `componentVisibility` tree property,
which a Lua `comp:setVisible(false)` never writes; `liveVisible` is the component's own flag.
All three are absent when a component was never parented — which happens silently whenever its
`componentLayerUid` matches no `uiPanelCanvasLayerUid`, a real hazard in hand-built panels.

**Make panel Lua headlessly testable: split dialogs from work.** Anything behind a modal dialog
(`utils.openFileWindow`, `askForTextInputWindow`) cannot run in the probe. Put the dialog in a thin
wrapper and the real work in a function taking plain scalars, then drive the worker with
`callLuaMethod`:

```lua
savePresetTo = function(path)        -- worker: testable, returns a status string
    ...
    return "ok " .. #frame
end
savePreset = function()              -- wrapper: dialog only, not covered by tests
    local f = utils.saveFileWindow("Save", File(""), "*.syx", true)
    if f:isValid() then savePresetTo(f:getFullPathName()) end
end
```

This buys a real assertion — the probe exercises CtrlrX's actual Lua VM, its real `File`/`MemoryBlock`
bindings, and the SysEx the panel truly emitted — rather than a simulation of them.

**Probing a binding's behaviour.** To settle what a JUCE wrapper actually does (overload picked,
0- vs 1-based, append vs replace), inject a throwaway `<luaMethod>` returning `type(x)` or a size into
a *scratch copy* of a panel and call it with `callLuaMethod`. Faster and more reliable than reading
luabind overload-resolution rules — that is how the traps in `references/lua-api.md` were established.

## Keep this skill current (self-evolution)

This skill is a living distillation, not a frozen document. When a session uncovers **durable,
generally-useful** knowledge about building CtrlrX panels that the skill is missing or gets wrong,
fold it back in before the learning evaporates — a future session (and the user) benefit from it.

**What qualifies** (capture it):
- A corrected fact — an enum value, attribute name, default, or behavior that turned out different
  from what a reference here says.
- A newly-learned format detail, token, callback, or gotcha that applies across panels.
- A reusable authoring pattern you had to work out (e.g. the right way to structure a particular
  control or wire a particular callback).
- A repeatable *mechanical* check or transform — that belongs in `scripts/panel_tool.py` as a new
  subcommand or check, so it's done by code next time, not re-derived.

**What does NOT qualify** (leave it out — it would just bloat the skill):
- One-off specifics of a single panel or a single device's SysEx implementation.
- Task trivia, file paths, or anything tied to one conversation.

**How to fold it in:**
1. **Confirm it against the source tree** first (see *Ground truth* above) — the skill should encode
   verified facts, not guesses. The user's build is the authority.
2. Put it in the **most specific place**: the relevant `references/*.md`, or this SKILL.md only if it
   changes the core model/workflow, or `scripts/panel_tool.py` if it's a mechanical check. Prefer
   *correcting existing text* over appending a new note that says the opposite.
3. Keep it lean. If you add something, see whether something stale should be removed or merged —
   the value of this skill is in being concise and correct, not exhaustive.
4. **Tell the user** what you changed and why, so they can veto or refine it.

Treat this the way you'd treat keeping a good README accurate: small, well-placed, verified edits as
you learn — not a changelog, and not speculative additions.

## Reference files (read the one you need)

- `references/panel-file-format.md` — full XML anatomy: the `<panel>` root attributes, the
  `<modulator>`/`<midi>`/`<component>` triad in detail, value maps in XML, resources, the
  `<luaManager>` schema, escaping, and the `.bpanelz` format. Read this when building a panel from
  scratch or when a panel won't load.
- `references/midi-and-values.md` — the message-type enum in depth, CC/Note/PC mappings, the **Multi
  Message list** syntax (14-bit, NRPN/RPN), the **SysEx formula** token language (checksums, value
  splits, Lua tokens), value expressions, and value maps. Read this for anything MIDI.
- `references/lua-api.md` — the Lua API surface panel authors use: globals, `CtrlrPanel`,
  `CtrlrModulator`, `CtrlrComponent`, `CtrlrMidiMessage`, sending MIDI, and the full **callback
  hook** catalogue with signatures and the `source` enum (avoiding feedback loops). Read this for
  any Lua work.
- `references/components.md` — the `uiType` catalogue: every component kind, what it's for, and which
  are *static* (hold no value / send no MIDI). Read this when choosing a control.
- `assets/template-panel.panel` — a minimal, known-good one-slider panel to copy from.

These references capture a snapshot; if one disagrees with the user's build, the **source tree wins**
(see *Ground truth* above).
