# Component (`uiType`) Catalogue

The `<component uiType="…">` attribute selects the widget kind. **Static** components hold no value
and send no MIDI (set `modulatorIsStatic="1"` and leave the `<midi>` type as `None`/9). When picking
a `uiType`, copy a same-`uiType` `<component>` block from a real panel (`Tests/fixture_*.panel`,
`Panels/`) — each type carries its own block of look attributes you don't want to invent. The full
set of types and their properties lives in `Source/UIComponents/CtrlrComponents/`.

## Sliders (continuous / stepped values)

| `uiType` | What it is | Use when |
|---|---|---|
| `uiSlider` | Standard slider/knob drawn by CtrlrX; style via `uiSliderStyle`. | A knob or fader without artwork. |
| `uiImageSlider` | Slider drawn from a **filmstrip** image (one frame per value). | A custom-looking knob. |
| `uiFixedSlider` | Slider snapping to a value map's discrete entries. | Discrete steps (e.g. waveform 1–5). |
| `uiFixedImageSlider` | Image slider with a fixed value set. | Discrete steps + custom artwork. |

`uiSliderStyle` values (strings): `LinearHorizontal`, `LinearVertical`, `LinearBar`,
`LinearBarVertical`, `Rotary`, `RotaryHorizontalDrag`, `RotaryVerticalDrag`,
`RotaryHorizontalVerticalDrag`, `TwoValueHorizontal/Vertical`, `ThreeValueHorizontal/Vertical`,
`IncDecButtons`. A **filmstrip** is a tall/wide PNG stacking one frame per value (a 0–127 knob needs
128 frames); set the frame width/height and orientation to match, and add the image as a resource.

> ⚠️ Gotcha — a slider's **visible range** is the *component* props **`uiSliderMin` / `uiSliderMax`**
> (on `<component>`), **not** the modulator's `modulatorMin` / `modulatorMax`. If you set only the
> modulator range, CtrlrX renders/clamps the knob to the component default `0..127`. Set **both** —
> e.g. a ±2400 bipolar knob needs `modulatorMin="-2400" modulatorMax="2400"` *and*
> `uiSliderMin="-2400" uiSliderMax="2400"`. (Verified against `Source/Core/CtrlrIDs.h`.)
>
> The reason to set both is not symmetry: **the component range is authoritative at runtime.**
> `CtrlrSlider::valueTreePropertyChanged` handles any `uiSliderMin`/`uiSliderMax`/`uiSliderInterval`
> change by calling `ctrlrSlider.setRange(...)` and then writing the result straight back —
> `owner.setProperty(Ids::modulatorMin/modulatorMax, ctrlrSlider.getMinimum()/getMaximum())`
> (`Sliders/CtrlrSlider.cpp:281-282`). So for a `uiSlider` the XML `modulatorMin`/`modulatorMax` is
> only the **cold-start** value, and Lua that rewrites `uiSliderMax` (a range that follows a type
> selector, say) is silently rewriting the declared modulator range too.

> ⚠️ Gotcha — **narrowing a slider's range strands the modulator's value above its own maximum.**
> JUCE's `setRange` constrains the slider without sending a notification, and
> `CtrlrModulator::setModulatorValue` does not clamp either, so after
> `comp:setPropertyString("uiSliderMax", "920")` a modulator sitting at 1023 reads **value 1023,
> max 920** — the display and the value disagree, and nothing warns you. Clamp explicitly in the
> same Lua:
>
> ```lua
> local ceiling = 920
> local mod = panel:getModulatorByName("cutoff")
> panel:getComponent("cutoff"):setPropertyString("uiSliderMax", tostring(ceiling))
> if mod:getModulatorValue() > ceiling then
>     mod:setModulatorValue(ceiling, true, false, true)   -- midi=false: do not echo to the device
> end
> ```
>
> `setModulatorValue(v, vst, midi, ui)` passes `!midi` to `setValueGeneric`, and `ui=true` marks the
> change `changedByProgram` (`Lua/CtrlrLuaManager.cpp:1681`) — so `(v, true, false, true)` is
> "update the value and the component, tell the device nothing". Measured with `CtrlrPanelProbe`.

> ⚠️ Gotcha — to show a **computed/mapped value on a knob's own LCD** (Hz from a log index, a
> nonlinear factor, an "Off" sentinel), a plain `uiSlider` won't do it: its text box only renders
> `number + uiSliderValueSuffix`. Only **`uiFixedSlider`** shows per-value text — its readout is
> `valueMap->getTextForIndex(value)`. Put the map in **`uiFixedSliderContent`**, one `TEXT=raw` line
> per entry (e.g. `221 Hz=42`). The map is indexed by **ordinal position**, so list entries in
> ascending raw order. If they start at raw `0` and run contiguously, ordinal == the raw byte and
> `getValue()`/`setValue()` keep operating on the raw value with no translation. It stays a rotary
> knob (`uiSliderStyle="Rotary"`). Value **expressions can't do this** — they only reshape the
> sent/received MIDI number, never the display text. Verified against
> `CtrlrSliderInternal::getTextFromValue`, `CtrlrFixedSlider::getTextForValue`, `CtrlrValueMap`, and
> headlessly via `CtrlrPanelProbe` (`finalState.modulators[].text` is the rendered readout).

> ⚠️ Gotcha — **`ordinal == raw` is a lucky special case, not a property of `uiFixedSlider`.** The
> readout is `getTextForIndex(ordinal)`: the modulator always holds the **ordinal**, never the device
> value. The moment the device values aren't `0..N` contiguous, the slider is as index-based as a
> `uiCombo`, and you must translate both ways yourself — ordinal→raw when sending, raw→ordinal when
> loading a patch. Two cases that bite:
> - a **signed or offset** range (±12 dB gain stored as −120..120): ordinal `i` means raw `lo + i`;
> - an **out-of-band sentinel** (speed 0–103 where `255` = "Off"). A fixed slider can only express one
>   contiguous ordinal run, so the sentinel is *unreachable* unless you append it as **one extra
>   ordinal past the top** of the map and resolve it through a lookup table.
>
> Keep a per-control `ordinal → raw` list beside the map and drive both directions from it. Assert it
> headlessly on the **device value** — the bytes you emit, or the patch image you write — not on
> `finalState.modulators[].value`, which is the ordinal: the two coincide in the easy case, so a test
> that only checks the modulator value passes happily while the device receives garbage.

## Buttons & choosers

| `uiType` | What it is | Use when |
|---|---|---|
| `uiToggleButton` | Two-state on/off button drawn by CtrlrX. | A simple on/off param (`modulatorMax="1"`). |
| `uiButton` | Text button, momentary or toggle. | A trigger ("Send patch") or labelled toggle. |
| `uiImageButton` | Button whose states are filmstrip frames. | Custom switches/LEDs. |
| `uiCombo` | Dropdown of named choices (fuzzy search). | Choosing from a named list — pairs with a value map. |
| `uiHyperlink` | Clickable link opening a URL. | Link to a manual / site. |

> ⚠️ **Why a button's `luaModulatorValueChange` doesn't fire — the one-line `uiButtonContent` trap.**
> A `uiButton`'s click handler is `valueMap->increment(); setComponentValue(valueMap->getCurrentNonMappedValue())`
> (`CtrlrButton.cpp:112-114`), and that valueMap is built by splitting **`uiButtonContent` on
> newlines**. The CtrlrX default is two lines (`"False\nTrue"`). Give the button a single-line
> caption — which looks like the natural thing to do for a trigger button labelled `"Send patch"` —
> and the map has exactly **one** entry, so `increment()` always lands on the same item and the
> component value is **permanently 0**. Everything downstream then fails silently:
> - `luaModulatorValueChange` never fires, because the value never *changes*;
> - a `NoteOn` modulator emits velocity **0** on every click (`9f 36 00`), which is a NoteOff by
>   MIDI convention, so the device ignores it and the button looks dead. Widening `modulatorMax`
>   does **not** fix this — the value still never leaves 0.
>
> Two lines make the value alternate `0`/`1` and the callback fire — but only on *alternate*
> clicks, which is wrong for a momentary trigger. **For anything trigger-shaped, use the mouse
> callbacks below instead**; they are independent of the valueMap and fire on every click.
> (Verified with `CtrlrPanelProbe`: one-line content → `9f 36 00`, no callback; mouse-up wiring →
> correct bytes on every click.)

> ⚠️ Gotcha — to run Lua on a **button press**, a `uiButton`/`uiToggleButton`'s
> `luaModulatorValueChange` does **not** reliably fire on click (see the valueMap trap above).
> Wire the trigger to the component's
> **mouse-up** callback instead: attribute **`componentLuaMouseUp`** (on `<component>`; the newer
> alias is `uiCustomMouseUpCallback`) set to your method name. The method receives the component's
> mouse-up args, so write it to ignore them (or use `function(mod, ...)`). Handled by the base
> component — verified in `Source/UIComponents/CtrlrComponents/CtrlrComponent.cpp`.
> Wire **only** mouse-up, not both `luaModulatorValueChange` *and* `componentLuaMouseUp`, to the same
> method: on some paths both fire and the method runs **twice** — harmless for an idempotent action
> (nav, re-send) but it silently cancels a state flip (e.g. a mode toggle nets zero). For a toggle,
> drive state from mouse-up alone (or read the control's own value), never from a double-wired method.

> 💡 Pattern — **`componentLuaMouseDown` is the paired attribute**, and together the two give you
> real **gate semantics** that no button value can express: NoteOn on press, NoteOff on release, so
> press-and-hold sustains. Set `midiMessageType="9"` on the modulator so the built-in message stays
> out of the way and Lua is the only sender:
>
> ```lua
> gateDown = function(...) panel:sendMidiMessageNow("9F 36 64") end   -- componentLuaMouseDown
> gateUp   = function(...) panel:sendMidiMessageNow("8F 36 00") end   -- componentLuaMouseUp
> ```
>
> Verified headlessly — `CtrlrPanelProbe`'s `click` action drives both, emitting
> `9f 36 64` then `8f 36 00` on every click. For a momentary blip, wire both to the same
> down/up pair as above; for a plain one-shot, wire mouse-up only.

> 💡 Pattern — **one Lua method for many buttons.** `modulatorCustomIndex` is a real CtrlrX
> property (`CtrlrIDs.h:58`, type Text) and is a good place to park per-control data, so a row of
> similar controls can share a single callback instead of one near-identical method each:
>
> ```lua
> shTrigger = function(mod, value, source)
>     local n = tonumber(mod:getProperty("modulatorCustomIndex"))   -- the note for THIS button
>     panel:sendMidiMessageNow(string.format("9F %02X 64", n))
> end
> ```
>
> Note it is stored as **Text**, so `tonumber()` it before arithmetic. (There is a matching
> `modulatorCustomIndexGroup` for grouping.)

> ⚠️ Gotcha — a `uiCombo` built from a plain `uiComboContent` list (`"Item A&#10;Item B&#10;…"`)
> is **index-based**: the modulator value is the **0-based item index**, and the displayed item is
> `list[value]` — *not* any raw value you may have intended. If your device wants non-contiguous raw
> bytes (e.g. `{0,2,4,6}`), either use a value map or keep the combo index-based and translate
> index→raw in the value-change Lua (`fw_valmap[fid][index]`). Setting the value to an out-of-range
> index shows the wrong (or clamped) item. Verified with `CtrlrPanelProbe` (`setValue`→`finalState`
> text). The `"Label=raw"` value-map form sets a *separate* mapped value; confirm which one your
> callback receives before relying on it.

> ⚠️ Gotcha — **an index-based value map must stay positionally dense: never leave a gap.**
> `CtrlrValueMap::parseString` gives a plain line `numericValue = line` (its line number), but it
> **`continue`s past blank and `__`-prefixed lines** without adding them to `values`
> (`UIComponents/CtrlrValueMap.cpp:70-76`). Meanwhile `getNonMappedMax()` is `values.size() - 1`
> (`:194`) — and that is what `CtrlrModulatorProcessor::setValueMap` writes into **`modulatorMax`**,
> so **the value map, not the XML, sets an index-based combo's range** — while `fillCombo` assigns
> item ids by **position** (`addItem(text, i+1)`, `:360`) and `setComponentValue(v)` selects id
> `v+1`. A skipped line therefore shifts every entry below it: the value you set no longer picks the
> item you meant, and the value read back is the *line number*, not the index. Adding a blank row as
> a placeholder for a reserved slot — the obvious way to leave a hole — is exactly the mistake. Use a
> visible placeholder label instead (and one with no `=` in it, which `parseString` would read as
> `label=value`).
>
> ⚠️ Gotcha — **never re-set a `uiCombo` from inside its own `luaModulatorValueChange`.**
> A page that re-renders from a selector's callback (pick part B -> redraw the part editor,
> selector included) re-enters `CtrlrCombo::setComponentValue` -- the same path behind the
> probe's documented double-send -- and CtrlrX **hangs**: the probe never returns a report.
> Measured with `CtrlrPanelProbe`. Skip the set when `getModulatorValue()` already equals the
> target; that covers every self-re-rendering combo at once.
>
> ⚠️ Gotcha — **new `uiComboContent` leaves the combo showing nothing** (JUCE item index -1)
> while its modulator still reads the old value, so a compare-and-skip then declines to fix it.
> Repaint it on the component without a callback:
> `panel:getComboComponent(name):setSelectedItemIndex(i, true)` -- the second argument is
> `dontNotify` (`CtrlrCombo.cpp:550`); the JUCE `NotificationType` overload is not bound.
>
> Reading one back: `comp:getProperty("uiComboContent")` returns a **real Lua string**, so
> `:sub()`/`#` work on it; `getPropertyString` returns a juce `String` userdata that is useless (and
> concatenating one with `..` segfaults CtrlrX).

## Statics (display only — no value, no MIDI)

| `uiType` | What it is |
|---|---|
| `uiLabel` | Text label (fixed text or a value readout). |
| `uiLCDLabel` | LCD-style digital display. |
| `uiImage` | Static image (logo, faceplate art). |
| `uiArrow` | Drawn arrow/shape. |
| `uiProgressBar` | Value shown as a filled bar (display only). |

> **`uiLabel` outline properties are both mis-named — read this before styling a label.**
>
> `uiLabelOutline` is **not** a boolean "draw a frame". CtrlrX maps it straight to
> `Label::setBorderSize(BorderSize<int>(value))` (`CtrlrLabel.cpp:186-189`) — JUCE's *text
> inset*, applied to all four sides. So `uiLabelOutline="4"` gives you 4 px of padding
> between the label's edge and its text, and nothing else. Because the inset is vertical
> too, a label needs height ≥ `2*inset + fontHeight` or the text is squeezed: a 12 px font
> at `uiLabelOutline="4"` wants a rect at least 20-22 px tall.
>
> `uiLabelOutlineColour` is what actually draws the frame — and it draws
> **unconditionally**. `LookAndFeel_V2::drawLabel` ends with
> `g.setColour(findColour(Label::outlineColourId)); g.drawRect(getLocalBounds());`
> (`juce_LookAndFeel_V2.cpp:1352-1359`), outside any `if`, and `LookAndFeel_V3` inherits
> it. Setting `uiLabelOutline="0"` does **not** suppress the frame.
>
> **The trap:** a generator that emits `uiLabelOutlineColour="ff000000"` as its "no
> outline" default gives every label a black 1 px frame. On a near-black panel that is
> easy to miss for a long time, but it shows up the moment a label sits on a lighter
> background — and it is very visible on a filled or coloured well. **To have no frame,
> emit a transparent outline colour (`"00000000"`).**

> **The background colour is `uiLabelBgColour`** (`CtrlrIDs.h:430`) — not
> `uiLabelBackgroundColour`. `CtrlrLabel::valueTreePropertyChanged` (`CtrlrLabel.cpp:178`)
> applies it live, so `comp:setProperty("uiLabelBgColour", "ffff3000", false)` from Lua
> repaints immediately. That makes a small `uiLabel` the cheapest **indicator LED**: give
> it an empty `uiLabelText`, a dark `uiLabelBgColour`, and drive the colour from
> `luaPanelMidiReceived`, using a `timer:setCallback`/`startTimer` pair to decay it back.

## Groups & views (containers — also static)

| `uiType` | What it is |
|---|---|
| `uiGroup` | Container; members move with the group. |
| `uiTabs` | Tabbed pages of controls; split a large editor into sections. |

## Misc / advanced (mostly Lua-driven)

| `uiType` | What it is | Notes |
|---|---|---|
| `uiMidiKeyboard` | On-screen keyboard for playing/sending notes. | |
| `uiCustomComponent` | Blank canvas you draw with Lua. | Drive via `uiCustomPaintCallback` + mouse callbacks. |
| `uiWaveform` | Audio waveform display with zoom/seek. | |
| `uiXYSurface` | 2-D pad controlling two parameters. | |
| `uiListBox` | Selectable list rendered via Lua. | Patch list / librarian. |
| `uiFileListBox` | File browser bound to a folder. | Selecting `.syx` banks, etc. |

## Quick chooser

```
on/off?                  -> uiToggleButton  (uiImageButton for custom art)
continuous value?        -> uiSlider        (uiImageSlider for custom art)
a few discrete steps?    -> uiFixedSlider / uiCombo
named choices?           -> uiCombo
caption / readout?       -> uiLabel / uiLCDLabel
container / sections?    -> uiGroup / uiTabs
play notes?              -> uiMidiKeyboard
draw something custom?   -> uiCustomComponent + Lua
list of patches/files?   -> uiListBox / uiFileListBox
```

## Shared property groups (every modulator)

Regardless of `uiType`, a modulator's attributes fall into: **Modulator** (`name`, `vstIndex`,
expressions, Lua callback names), **Component generic** (`componentVisibleName`, label visibility,
colours, `componentRectangle`, image resource, frame size), **Component** (type-specific look like
`uiSliderStyle`), and **MIDI** (the `<midi>` child). `modulatorMin`/`modulatorMax` (raw range) live
on `<modulator>`; mapping their *meaning* to names/units is the value-map / expression system in
`midi-and-values.md`.
