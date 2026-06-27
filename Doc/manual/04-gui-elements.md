[← 03 Your First Panel](03-first-panel.md) | [Index](README.md) | Next: [05 — Value Mapping →](05-value-mapping.md)

---

# 04 — GUI Elements

> **TL;DR**
> - Right-click the canvas → **Add component**. Items are grouped into **Sliders, Buttons, Statics, Groups and views, Misc**.
> - **Image** variants (uiImageSlider, uiImageButton) use a *filmstrip* image; plain variants are drawn by CtrlrX.
> - **Static** elements (labels, image, arrow, group, tabs, progress bar) don't produce a MIDI value.
> - Jump to the table for your category below.

[← 03 Your First Panel](03-first-panel.md) · [Index](README.md) · Next: [05 — Value Mapping →](05-value-mapping.md)

---

This is the catalogue of every control type you can add. The grouping here matches the **Add
component** menu exactly. Each control's internal type name (e.g. `uiSlider`) is what you'll see in
the menu and in the saved panel XML.

> 💡 Tip: Not sure which to use? The short answer for most parameters: a **uiSlider** (knob/fader) for
> continuous values, a **uiToggleButton** for on/off, a **uiCombo** for a list of named choices.

## Sliders

Continuous (or stepped) value controls — knobs and faders.

| Type | What it is | Reach for it when… |
|---|---|---|
| **uiSlider** | A standard slider/knob drawn by CtrlrX. Style is configurable (see below). | You want a knob or fader without supplying artwork. |
| **uiImageSlider** | A slider whose appearance is a **filmstrip image** (one frame per value/angle). | You want a custom-looking knob (e.g. a synth-style rotary). |
| **uiFixedSlider** | A slider that snaps to a fixed set of values (a value map). | The parameter has discrete steps (e.g. waveform 1–5). |
| **uiFixedImageSlider** | Image-based slider with a fixed value set. | Discrete steps *and* custom artwork. |

**uiSlider styles** (the *Slider style* property): `LinearHorizontal`, `LinearVertical`,
`LinearBar`, `LinearBarVertical`, `Rotary`, `RotaryHorizontalDrag`, `RotaryVerticalDrag`,
`RotaryHorizontalVerticalDrag`, `TwoValueHorizontal/Vertical`, `ThreeValueHorizontal/Vertical`,
`IncDecButtons`.

> 🔗 Deeper: **Filmstrip images** — a tall (or wide) PNG that stacks one frame per value. A 0–127
> knob needs 128 frames. Set **Frame width/height** and **Image frame orientation** (vertical/
> horizontal) to match your artwork. Upload the image via the **Resources** tab first
> ([Chapter 2](02-core-concepts.md)).

## Buttons

| Type | What it is | Reach for it when… |
|---|---|---|
| **uiToggleButton** | A two-state on/off button drawn by CtrlrX. | A simple on/off parameter. |
| **uiButton** | A text button; can be momentary or toggle. | A trigger (e.g. "Send patch") or a labelled toggle. |
| **uiImageButton** | A button whose states are frames of a filmstrip image. | Custom-looking switches/LEDs. |
| **uiCombo** | A dropdown list of named choices (with fuzzy search). | Choosing from a named list (e.g. LFO shape). |
| **uiHyperlink** | A clickable link that opens a URL. | Linking to a manual, your site, etc. |

> 💡 Tip: **uiCombo** is the friendliest way to expose an enumerated parameter, because each MIDI
> value gets a human label. Its choices come from a value map — see [Chapter 5](05-value-mapping.md).

## Statics

Display-only elements. **These are static**: they hold no MIDI value and send nothing.

| Type | What it is | Reach for it when… |
|---|---|---|
| **uiLabel** | A text label (can display a value or fixed text). | Captions, headings, readouts. |
| **uiLCDLabel** | An LCD-style digital display. | A "screen" look for values. |
| **uiImage** | A static image. | Logos, decoration, faceplate art. |
| **uiArrow** | A drawn arrow/shape. | Visual flow, decoration. |

## Groups and views

Containers that organize other controls. Also static.

| Type | What it is | Reach for it when… |
|---|---|---|
| **uiGroup** | A container; members move together with the group. | Keeping a cluster of related controls as a unit. |
| **uiTabs** | Tabbed pages of controls. | Splitting a large editor into sections. |

> 🔗 Deeper: organizing with groups, tabs, and layers is covered in [Chapter 2](02-core-concepts.md).

## Misc

Specialized and advanced components.

| Type | What it is | Reach for it when… |
|---|---|---|
| **uiMidiKeyboard** | An on-screen MIDI keyboard for playing/sending notes. | You want to trigger notes from the panel. |
| **uiCustomComponent** | A blank canvas you draw on with Lua (`uiCustomPaintCallback`). | You need a custom widget (e.g. an envelope graph). |
| **uiWaveform** | An audio waveform display with zoom/seek. | Showing/playing audio (e.g. a sample preview). |
| **uiXYSurface** | A 2-D pad controlling two parameters at once. | X/Y control (e.g. a vector/morph pad). |
| **uiListBox** | A selectable list of items, rendered via Lua. | A patch list, a librarian. |
| **uiFileListBox** | A file browser bound to a folder. | Selecting files (e.g. `.syx` banks). |
| **uiProgressBar** | A value shown as a filled bar (static — display only). | Progress / level indication. |

> 🔗 Deeper: the **uiCustomComponent**, **uiListBox**, **uiWaveform**, and **uiXYSurface** are driven
> almost entirely from Lua callbacks. See the callback table in the
> [Lua reference](lua/02-lua-reference.md#callback-hooks).

## Properties every modulator shares

No matter the type, you'll see these groups in the Properties panel:

| Group | Examples |
|---|---|
| **Modulator** | Name, VST index (for host automation) |
| **Component generic** | Visible name, name-label visibility, background/colours, position & size, image resource, frame width/height |
| **Component** | Type-specific look (slider style, value position, min/max, tabs orientation…) |
| **MIDI** | MIDI message type, channel, number, value, multi list, SysEx formula |

> ⚠️ Gotcha: **Minimum/Maximum value** lives under *Component* and defines the raw value range. The
> *meaning* of those numbers (mapping them to Hz, names, etc.) is a separate system — see
> [Chapter 5](05-value-mapping.md).

## Choosing the right control — quick guide

```
on/off?                      → uiToggleButton  (or uiImageButton for custom art)
continuous value?            → uiSlider        (or uiImageSlider for custom art)
a few discrete steps?        → uiFixedSlider / uiCombo
named choices?               → uiCombo
just a caption/readout?      → uiLabel / uiLCDLabel
play notes?                  → uiMidiKeyboard
draw something custom?       → uiCustomComponent + Lua
list of patches/files?       → uiListBox / uiFileListBox
```

---

[← 03 Your First Panel](03-first-panel.md) | [Index](README.md) | Next: [05 — Value Mapping →](05-value-mapping.md)
