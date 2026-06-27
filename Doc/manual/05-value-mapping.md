[← 04 GUI Elements](04-gui-elements.md) | [Index](README.md) | Next: [06 — MIDI Basics →](06-midi-basics.md)

---

# 05 — Value Mapping

> **TL;DR**
> - Every control has a raw **value** between **Minimum** and **Maximum** (often 0–127, the MIDI range).
> - **Non-mapped value** = the raw number. **Mapped value** = what it *means* (Hz, a name, etc.).
> - A **value map** translates raw values ↔ display text/values. Combos and fixed sliders use it for named choices.
> - In Lua: `getValue()`/`setValue()` for raw, `getValueMapped()`/`setValueMapped()` for mapped.

[← 04 GUI Elements](04-gui-elements.md) · [Index](README.md) · Next: [06 — MIDI Basics →](06-midi-basics.md)

---

## Raw range: Minimum and Maximum

The simplest mapping is just a range. Under the **Component** property group:

- **Minimum value** — the lowest raw value (commonly `0`).
- **Maximum value** — the highest raw value (commonly `127` for a 7-bit MIDI parameter, `16383` for a
  14-bit one, `1` for an on/off switch).

The raw value is what the control stores and, by default, what it sends as MIDI. For our example
cutoff knob we used `0–127`.

> ⚠️ Gotcha: Match **Maximum value** to your MIDI resolution. A 7-bit CC tops out at `127`; using a
> 14-bit (Multi/NRPN) parameter means `16383`. See [Chapter 8](08-sending-receiving.md).

## Mapped vs. non-mapped values

CtrlrX separates two ideas:

| Term | Meaning | Example |
|---|---|---|
| **Non-mapped value** | The raw number the control holds. | `64` |
| **Mapped value** | The human-meaningful value it represents. | `"Sawtooth"`, or `2000 Hz` |

For a plain knob the two are usually identical. They diverge when you attach a **value map**.

## Value maps (named choices and lookup tables)

A **value map** is a list of pairs: *raw value → display text* (and optionally a numeric mapped
value). It's what turns a dropdown of numbers into a dropdown of names.

**Goal:** give a combo box named choices.

**Steps**
1. Add a **uiCombo** (or **uiFixedSlider**) and select it.
2. Find its **value map / item list** property (the list editor in Properties).
3. Enter pairs, one per line, e.g.:
   ```
   0=Sine
   1=Triangle
   2=Sawtooth
   3=Square
   ```
4. Set **Maximum value** to match the highest index (`3` here).

Now the control shows `Sine…Square`, but still sends `0…3` as MIDI.

**How it works**

Internally a value map (`CtrlrValueMap`) keeps an ordered set of *non-mapped* values, each paired with
a *mapped* value and a *text*. The component shows the text; the processor sends the value. The same
machinery powers `uiFixedSlider`/`uiFixedImageSlider`, which step through the map's entries.

## Reading and writing values in Lua

This is where the distinction pays off:

```lua
local m = panel:getModulatorByName("lfoShape")

m:getValue()           -- raw value, e.g. 2
m:getValueMapped()     -- mapped value (number) for that entry
m:getValueNonMapped()  -- explicit raw value
-- integer variants exist: getValueInt(), getValueMappedInt(), getValueNonMappedInt()

m:setValue(2)          -- set raw value
m:setValueMapped(...)  -- set by mapped value
m:setValueNonMapped(...)
```

Ranges, too:

```lua
m:getMinModulatorValue(), m:getMaxModulatorValue()  -- raw range
m:getMinMapped(),        m:getMaxMapped()           -- mapped range
m:getMinNonMapped(),     m:getMaxNonMapped()
```

> 🔗 Deeper: the full modulator API (and the older `getModulatorValue`/`setModulatorValue` aliases
> you'll see in existing panels) is in the [Lua reference](lua/02-lua-reference.md#ctrlrmodulator).

## Expression properties (math without Lua)

For simple, computed mappings you don't need Lua at all. Each modulator has three **expression**
properties (in the *Modulator* property group) that are evaluated as math formulas. They default to
pass-through, so by default the modulator value *is* the MIDI value.

| Property (label in the editor) | Direction | Default |
|---|---|---|
| **Expression to evaluate when calculating the midi message value from the modulator value** | modulator value → outgoing MIDI value | `modulatorValue` |
| **Expression to evaluate when calculating the modulator value from the midi message value** | incoming MIDI value → modulator value | `midiValue` |
| **Expression to evaluate when calculating the modulator value from midi controller message** | controller (MIDI-learn) input → modulator value | `value` |

**Variables you can use** in these formulas: `modulatorValue`, `modulatorMappedValue`, `midiValue`,
`midiNumber`, `vstIndex` (plus `panel.<prop>` and `global.<n>` scopes).

**Functions available**: `abs`, `ceil`, `floor`, `mod`, `fmod`, `pow`, `min`, `max`, the comparisons
`eq`/`lt`/`lte`/`gt`/`gte`, the bit helpers `setBit`/`isBitSet`/`clearBit`/`getBitRangeAsInt`/
`setBitRangeAsInt`, and `setGlobal`, plus ordinary arithmetic (`+ - * /`).

**Example** — halve the value on the way out and double it on the way back in:

```
forward (to MIDI):   floor(modulatorValue / 2)
reverse (from MIDI): midiValue * 2
```

> 💡 Tip: Use expressions for linear/scaling/bit-twiddling. Reach for Lua (below) only when the
> relationship is a lookup table or needs real logic.

## When you need an arbitrary lookup

Sometimes a parameter's raw MIDI value maps to a non-linear real-world value (classic example: an
envelope time where MIDI `0–255` maps to `0…60` seconds non-linearly). Two approaches:

1. **Value map** — if the relationship is a fixed list, enter it as pairs.
2. **Lua + a table** — for computed relationships, keep a Lua table and a helper function:
   ```lua
   EnvTimes = { 0, 0.002, 0.005, 0.01, --[[ … ]] }
   function timeForMidi(v) return EnvTimes[v + 1] end   -- 0-based MIDI → seconds (1-based table)
   ```

### Tying the lookup to a control

That helper function does nothing on its own — a **callback** has to call it. Pick the callback that
matches what you're doing (all three live in the modulator's *Modulator* property group, each as a
method dropdown with **Add new method** / **Edit** buttons — see [Chapter 7](07-making-responsive.md)):

| You want to… | Use the callback labelled… | (internal name) |
|---|---|---|
| Show the real-world value on a label/LCD when the knob moves | **Called when the modulator value changes** | `luaModulatorValueChange` |
| Convert the modulator value into the byte(s) you send | **Called to calculate the MIDI value to send** | `luaModulatorGetValueForMIDI` |
| Convert an incoming MIDI value back into the modulator value | **Called to calculate new modulator value from a MIDI value** | `luaModulatorGetValueFromMIDI` |

For example, to display the envelope time whenever the knob moves, create a method, assign it to
**Called when the modulator value changes**, and have it do:

```lua
envTimeChanged = function(mod, value)
    local seconds = timeForMidi(value)
    panel:getModulatorByName("envTimeLabel"):getComponent()
         :setComponentText(string.format("%.3f s", seconds))
end
```

> 🔗 Deeper: how to create and assign these methods is in [Chapter 7](07-making-responsive.md); the
> full callback list (and exact argument lists) is in the
> [Lua reference](lua/02-lua-reference.md#callback-hooks).

---

[← 04 GUI Elements](04-gui-elements.md) | [Index](README.md) | Next: [06 — MIDI Basics →](06-midi-basics.md)
