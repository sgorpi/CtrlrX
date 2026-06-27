[← Manual Index](../README.md) | Next: [Lua API Reference →](02-lua-reference.md)

---

# Lua Guide

> **TL;DR**
> - CtrlrX embeds **Lua 5.1**. Lua is small, fast, and easy to pick up if you know any language.
> - You write **methods** and attach them to **callbacks** (value change, MIDI received, paint…).
> - Inside a method you have ready-made globals: `panel`, `utils`, `timer`, `devices`, and more.
> - This page teaches the language; the [API Reference](02-lua-reference.md) lists what CtrlrX exposes.

[← Manual Index](../README.md) · Next: [Lua API Reference →](02-lua-reference.md)

---

## Why Lua, and which version

Lua is the scripting layer that turns a static panel into a smart one: compute values, drive
displays, parse SysEx, build a librarian. CtrlrX embeds **Lua 5.1** — so use the **5.1** manual and
examples (later 5.x versions differ in a few places).

**Learn Lua:**
- Reference manual (5.1): <https://www.lua.org/manual/5.1/>
- *Programming in Lua* (1st ed., matches 5.0/5.1, free online): <https://www.lua.org/pil/contents.html>
- Community wiki & recipes: <http://lua-users.org/wiki/>
- String formatting tutorial: <http://lua-users.org/wiki/StringLibraryTutorial>

> 💡 Tip: Spell it **Lua** (a Portuguese word, "moon"), not "LUA" — it isn't an acronym.

## How Lua fits into CtrlrX

You don't write a `main()`. You write **methods** — named functions — and attach each to a **callback**
that CtrlrX calls at the right time:

```lua
-- attached to a knob's "Called when the modulator value changes" callback
cutoffChanged = function(mod, value)
    console("cutoff is now " .. value)
end
```

### Linking a method to a callback

You do **not** type a function name into the callback field. Each callback in the Properties panel is
a **dropdown of existing methods** with a small toolbar beside it (**Add new method**, **Edit selected
method**, **clear**). So either:

**Steps (create a new method)**
1. Select the modulator (or panel/component) that owns the callback.
2. Find the callback row — e.g. **Called when the modulator value changes**.
3. Click **Add new method**, give it a name (e.g. `cutoffChanged`), **OK**.
4. **Select it in that callback's dropdown** — creating it does *not* assign it.
5. Click **Edit selected method** to open the [LUA Editor](../09-debugging.md) and fill in the body.

**Or reuse an existing method:** just pick it from the dropdown. One method can be assigned to several
callbacks.

> ⚠️ Gotcha: the method's function name (the `name = function(...)` line) is generated for you from the
> method name and **must stay matching** it. Rename a method only via the editor's method list, not by
> editing the `function` line.

### The callback signature is fixed

When you create a method for a callback, CtrlrX pre-fills the **correct parameter list** for that
callback — you just write the body. For *Called when the modulator value changes* that's
`function(mod, value)` (some give a third `source` argument); for a MIDI-received callback it's
`function(midi)`; for a paint callback `function(component, graphics)`; and so on. Don't change the
parameter list — if you need different data, get it inside the body (e.g. `panel:getModulatorByName…`).

> 🔗 Deeper: every callback and its exact arguments is listed under
> [Callback hooks](02-lua-reference.md#callback-hooks); the wiring UI is walked through in
> [Chapter 7](../07-making-responsive.md). Output and errors show in the
> [Lua console](../09-debugging.md).

## Lua in 10 minutes (for programmers)

### Comments

```lua
-- a single-line comment
--[[ a
     multi-line comment ]]
```

### Variables and types

Variables are dynamically typed and need no declaration. **They are global by default** — use `local`
to scope them (recommended, to avoid surprises):

```lua
local name = "Fred"      -- string
local count = 5          -- number (Lua 5.1 has one numeric type)
local on = true          -- boolean
local nothing = nil      -- absence of value
```

`nil` means "no value" (like null). Reading an unset variable yields `nil`; indexing `nil` is the
most common runtime error you'll hit.

### Strings

```lua
local s = "cutoff"
local joined = s .. " = " .. tostring(64)   -- ".." concatenates
local n = #s                                 -- length (6)
local up = string.format("%s=%d", s, 64)     -- formatting
```

### Conditionals

```lua
if value == 0 then
    -- ...
elseif value < 64 then
    -- ...
else
    -- ...
end
```

Operators: `==` `~=` (not equal) `<` `<=` `>` `>=`, and `and` / `or` / `not`. **Lua is
case-sensitive** — `If`/`THEN` won't work.

### Loops

```lua
for i = 0, 127 do            -- numeric: 0..127 inclusive
    -- ...
end

for index, v in ipairs(myArray) do  -- array iteration
    -- ...
end

while condition do  end
repeat  until condition
```

### Functions

```lua
local function clamp(x, lo, hi)
    if x < lo then return lo end
    if x > hi then return hi end
    return x
end
```

### Tables (the only data structure)

Tables are Lua's arrays, dictionaries, and objects all in one:

```lua
local list = { 10, 20, 30 }        -- array; list[1] == 10 (1-based!)
local map  = { sine = 0, saw = 2 } -- dictionary; map.saw == 2

list[#list + 1] = 40               -- append
```

> ⚠️ Gotcha: Lua arrays are **1-based**. `list[1]` is the first element. When mapping a 0-based MIDI
> value into a table, add 1: `times[midiValue + 1]`.

### Method-call syntax (the colon)

`obj:method(args)` is sugar for `obj.method(obj, args)` — it passes `obj` as the implicit `self`. All
CtrlrX objects are used with the **colon**:

```lua
panel:getModulatorByName("filterCutoff"):setValue(100)
```

## A few CtrlrX-specific habits

- **Output to the console** for debugging: `console("hi")` or `console(string.format("v=%d", v))`.
- **Reach the panel** via the global `panel`; reach a control via
  `panel:getModulatorByName("Name")` — the name is the modulator's **Name** property and is
  **case-sensitive**.
- **A modulator vs. its component** ([Chapter 2](../02-core-concepts.md)): `mod:getComponent()` gets
  the visual; many display tweaks live on the component.
- **Don't fight the callback signature** — the parameter list is pre-wired; just fill the body.
- **Guard against MIDI feedback loops** — `luaModulatorValueChange` gives you a `source` so you can
  ignore changes that came *from* MIDI in (see [Chapter 8](../08-sending-receiving.md)).

## Sending MIDI from a method (using variables)

`panel:sendMidiMessageNow(...)` accepts either a **hex string** or a **`CtrlrMidiMessage`** object. The
string form (`"90 3C 64"`) is handy for fixed messages — but when your note number, velocity, channel,
etc. live in **variables**, you do **not** have to hand-build a string. Use one of these (all take
plain numbers):

```lua
local note, vel, ch = 60, 100, 1

-- 1) Table of byte numbers (cleanest). First byte must be a status byte (>= 0x80).
panel:sendMidiMessageNow(CtrlrMidiMessage({ 0x90 + (ch - 1), note, vel }))

-- 2) JUCE MidiMessage from numbers (status, data1, data2, timestamp), sent via sendMidi:
panel:sendMidi(MidiMessage(0x90 + (ch - 1), note, vel, 0), 0)

-- 3) Build the hex string only if you prefer strings:
panel:sendMidiMessageNow(string.format("%02X %02X %02X", 0x90 + (ch - 1), note, vel))

-- 4) Construct once, then set fields from variables:
local m = CtrlrMidiMessage("90 00 00")
m:setChannel(ch); m:setNumber(note); m:setValue(vel)
panel:sendMidiMessageNow(m)
```

> 💡 Tip: The MIDI **channel** is encoded in the low nibble of the status byte — channel 1 is `0x90`,
> channel 2 is `0x91`, … hence `0x90 + (ch - 1)`. Or just call `setChannel(ch)` (1–16) as in option 4.

> ⚠️ Gotcha: Only the *string* constructor needs a string. Options 1, 2, and 4 take numbers directly,
> so you never need `tostring()`/`string.format()` just to inject a variable.

> 🔗 Deeper: `CtrlrMidiMessage` (constructors, `setChannel`/`setNumber`/`setValue`) and `sendMidi`
> vs `sendMidiMessageNow` are in the [reference](02-lua-reference.md#sending-midi); building NRPN/SysEx
> is in [Chapter 8](../08-sending-receiving.md).

## A complete small example

A combo (`lfoShape`) that, when changed, updates a label (`lfoLabel`) and logs:

```lua
-- assigned to lfoShape's "modulator value changed" callback
lfoShapeChanged = function(mod, value)
    local names = { [0]="Sine", [1]="Triangle", [2]="Saw", [3]="Square" }
    local text = names[value] or "?"
    panel:getModulatorByName("lfoLabel"):getComponent():setComponentText(text)
    console(string.format("LFO shape -> %s (%d)", text, value))
end
```

> 🔗 Deeper: every method used here (`getModulatorByName`, `getComponent`, `setComponentText`,
> `setValue`, …) is documented in the [API Reference](02-lua-reference.md).

---

[← Manual Index](../README.md) | Next: [Lua API Reference →](02-lua-reference.md)
