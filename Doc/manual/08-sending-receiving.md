[← 07 Making Responsive](07-making-responsive.md) | [Index](README.md) | Next: [09 — Debugging →](09-debugging.md)

---

# 08 — Sending & Receiving MIDI

> **TL;DR**
> - **Multi** messages send a *sequence* — use them for 14-bit values, **NRPN/RPN**, or any combo.
> - **SysEx** messages use a **formula**: a byte template with tokens that get filled in at send time.
> - **Receiving**: CtrlrX matches incoming MIDI to a modulator (by type/channel/number) and updates it automatically; use callbacks for custom handling.

## Contents

- [Multi messages (14-bit, NRPN/RPN, sequences)](#multi-messages-14-bit-nrpnrpn-sequences)
- [SysEx messages (formulas & tokens)](#sysex-messages-formulas--tokens)
- [Receiving MIDI](#receiving-midi)
- [Sending MIDI from Lua](#sending-midi-from-lua)
- [Bulk dumps — many modulators in one message](#bulk-dumps--many-modulators-in-one-message)
- [Keeping a patch image (shadow state)](#keeping-a-patch-image-shadow-state)
- [Back to the example panel](#back-to-the-example-panel)

---

[Chapter 7](07-making-responsive.md) covered the simple types (CC, Note, PC…). This chapter handles
the two powerful ones — **Multi** and **SysEx** — and explains how a panel *receives* MIDI.

## Multi messages (14-bit, NRPN/RPN, sequences)

Set **MIDI message type = Multi** and fill in the **Multi Message list**. The list is a sequence of
messages separated by **colons** `:`. Each message is comma-separated:

```
Type[,Number[,Value]] : Type[,Number[,Value]] : …
```

For **Number** and **Value** you can use:

| Token | Means |
|---|---|
| `Direct`, `Default`, `Value`, or `-1` | Use the **modulator's current value**. |
| `Number`, `CtrlNumber`, or `-2` | Use the **modulator's MIDI number**. |
| a decimal (e.g. `74`) | A literal number. |
| a 1–2 digit hex (e.g. `0x40` → write `40`) | A literal hex byte. |

### Example: a 14-bit CC (MSB + LSB)

A 0–16383 parameter split across CC 1 (MSB) and CC 33 (LSB) — set **Maximum value = 16383** and:

```
CC,1,Direct : CC,33,Direct
```

Each message takes the modulator's value and CtrlrX places the high/low 7 bits appropriately when the
value exceeds 127.

### Example: NRPN

NRPN selects a parameter with CC 99/98, then sets it with CC 6 (and optionally CC 38):

```
CC,99,2 : CC,98,10 : CC,6,Direct
```

(Parameter 2/10, data = the modulator value.)

> ⚠️ Gotcha — legacy format: older panels use a longer token form with `ByteValue`,
> `MSB7bitValue`, `LSB7bitValue` (e.g. `CC,ByteValue,MSB7bitValue,28,-1:…`). CtrlrX still understands
> these — it routes them through the SysEx engine for backward compatibility — but for **new** panels
> prefer the shorter `Type,Number,Value` form above.

### Example: a sequence of fixed messages

A "panic"-style button (type `None`, triggered from Lua or as a Multi) could send:

```
CC,123,0 : CC,120,0
```

(All notes off, all sound off.)

## SysEx messages (formulas & tokens)

System Exclusive lets a device expose parameters that have no standard MIDI message. Set **MIDI
message type = SysEx** and write a **SysEx formula**: the raw bytes of the message written as
two-character **hex** (e.g. `F0 7D 12 … F7`), with short **token codes** wherever a byte should be
filled in dynamically at send time.

The tokens are **short codes you type into the formula** — *not* long names. Each code stands for a
byte (or bytes) that CtrlrX substitutes when the message is sent: `xx` becomes the modulator's value,
`yy` becomes the MIDI channel, a checksum code becomes the computed checksum, and so on. You can type
them by hand, or (in the SysEx editor) click a byte cell and use **Add Token** to insert the right
code from a menu.

The codes you'll use most:

| Code | Byte it inserts |
|---|---|
| `xx` | The modulator's value as one 7-bit byte. |
| `MS` / `LS` | The value's high / low **7 bits** (for values > 127, e.g. a 14-bit parameter). |
| `ms` / `ls` | The value's high / low **4-bit** nibble. |
| `yy` | The MIDI channel (7-bit). `0y` packs it into a low nibble (4-bit). |
| `r1`–`r4` | Roland-style address/value **byte splits** (e.g. JV/JD series). |
| `q0`–`q3` / `Q0`–`Q3` | A 16-bit value as four 4-bit nibbles, LSB-first / MSB-first. |
| `tp` / `tb` | The current program / bank number. |
| `k…` / `o…` / `p…` / `n…` | The current value of a panel [global variable](10-distribution.md#global-variables--persistent-state) (see note below). |
| `ii` | Ignore this byte when *matching incoming* SysEx (wildcard). |
| `f0` / `f7` | Literal SysEx start `F0` / end `F7` (typing `F0`/`F7` works too). |
| `Xn` `zn` `wn` `On` `tc` | A **checksum** over the *n* bytes preceding it — XOR (`Xn`), 2's-complement (`zn`; Roland/Yamaha), simple sum (`wn`), 1's-complement (`On`, an o, not a zero), Technics (`tc`). |
| `u…` / `v…` | A byte computed by Lua / by a sub-formula. |

> 💡 The digit after a checksum code is how many preceding bytes it covers. `X3` = XOR of the 3
> bytes immediately before the checksum byte.

> 💡 **Global-variable tokens** (`k…` / `o…` / `p…` / `n…`). A panel has **64 global variables** — a
> shared array of integers (indices 0–63) that your Lua methods and value expressions read and write
> with `panel:getGlobalVariable(i)` / `panel:setGlobalVariable(i, v)` (see
> [Chapter 10](10-distribution.md#global-variables--persistent-state)). One of these tokens inserts the
> *current value* of one global as a byte in the message. The **letter selects a bank of 16** and the
> **trailing hex digit (`0`–`f`) selects within it**:
>
> | Token | Global index |
> |---|---|
> | `k0`–`kf` | 0–15 |
> | `o0`–`of` | 16–31 |
> | `p0`–`pf` | 32–47 |
> | `n0`–`nf` | 48–63 |
>
> So `k5` emits the value of global `5`; `o0` emits global `16`. This is how you put a value into a
> SysEx message that **isn't** the modulator's own value — e.g. a device ID, a bank/part number, or a
> shared "current edit buffer" index your script maintains. Easiest is **Add Token → Global variable**,
> which writes the correct code for you.

**Example 1** — a fictional device (manufacturer `7D`), set parameter `0x12` to the knob value, with
an XOR checksum over the three bytes before it:

```
F0 7D 12 xx X3 F7
```

When the knob moves, CtrlrX substitutes `xx` with the value and replaces `X3` with `7D ^ 12 ^ value`,
then sends the complete `F0 … F7` message.

**Example 2** — a 14-bit parameter split into MSB + LSB bytes (set **Maximum value = 16383**):

```
F0 43 10 4A MS LS F7
```

Here `MS`/`LS` carry the high/low 7 bits of the value.

> ⚠️ These short codes are the **SysEx formula** vocabulary and are *different* from the enum values
> (`ByteValue`, `MSB7bitValue`, …) used in the source code. In a SysEx formula, type `xx`, not `ByteValue`.

> 🔗 Deeper: the code→byte mapping is `CtrlrSysexProcessor::sysExIdentifyToken`
> ([Source/Core/](../../Source/Core/CtrlrSysexProcessor.cpp)); token substitution/checksums live in the
> same file and `CtrlrSysexToken`. The full list (including every checksum and split) is on the
> [SysEx Token List](https://github.com/damiensellier/CtrlrX/wiki/SysEx-Token-List) wiki page. For
> devices too gnarly for the token language, compute bytes in Lua and send them with
> `panel:sendMidiMessageNow(...)` (see below and the
> [Lua reference](lua/02-lua-reference.md#sending-midi)).

## Receiving MIDI

The reverse direction — gear → panel — mostly happens **automatically**.

**How it works**

When MIDI arrives on the selected **Input** device, CtrlrX's *input comparator* tries to match it to a
modulator by **message type + channel + number** (and, for SysEx, by matching the data pattern). On a
match, that modulator's value is updated and its component repaints — your on-screen knob follows the
hardware.

For this to work, the receiving modulator must be configured with the **same** MIDI type/channel/
number it sends. In practice, a control that sends CC 74 on channel 1 will also *receive* CC 74 on
channel 1 with no extra setup.

### Custom receive handling with Lua

When automatic matching isn't enough (e.g. parsing a bulk SysEx dump, or one message updating many
controls), use a panel callback:

```lua
-- assign this to the panel's "midi received" callback (luaPanelMidiReceived)
panelMidiReceived = function(midi)
    if midi:getMidiMessageType() == CC and midi:getNumber() == 74 then
        panel:getModulatorByName("filterCutoff"):setValue(midi:getValue())
    end
end
```

| Callback | Fires when |
|---|---|
| `luaPanelMidiReceived` | Any MIDI message is received. |
| `luaPanelMidiMultiReceived` | A multi-part message is received. |
| `luaModulatorValueChange` | A modulator's value changes (with a `source` arg telling you *why* — host, MIDI in, GUI, Lua…). |

> 🔗 Deeper: the `source` enum on `luaModulatorValueChange` (initial / host / midiIn / controller /
> gui / lua / program / link / unknown) lets you avoid feedback loops — e.g. *don't* re-send MIDI when
> the change *came from* MIDI in. See the [Lua reference](lua/02-lua-reference.md#callback-hooks) and
> the worked examples on the
> [Source filter with LUA scripts](https://github.com/damiensellier/CtrlrX/wiki/Source-filter-with-LUA-scripts)
> wiki page.

## Sending MIDI from Lua

You're not limited to a control's configured message. From any method:

```lua
panel:sendMidiMessageNow("F0 7D 12 40 F7")          -- a raw SysEx string
panel:sendMidiMessageNow(CtrlrMidiMessage("B0 4A 7F"))  -- a CtrlrMidiMessage
```

> 🔗 Deeper: `sendMidiMessageNow`, `sendMidi` (with a timestamp), and constructing `CtrlrMidiMessage`
> objects are documented in the [Lua reference](lua/02-lua-reference.md#sending-midi).

## Bulk dumps — many modulators in one message

> **TL;DR** — Instead of packing a big SysEx dump byte by byte, tag the modulators with a **custom
> index property**, then let CtrlrX serialize all their values into one block with
> `panel:getModulatorValuesAsData(...)` (to send) and unpack an incoming dump with
> `panel:setModulatorValuesFromData(...)` (to receive). You choose how each value is encoded.

Many synths load/save a whole patch as a single SysEx "bulk dump": a header, then one byte (or a
nibble pair, or two bytes…) per parameter, in a fixed order, then a checksum and `F7`. Wiring that up
control-by-control is painful. CtrlrX gives you two panel methods that walk your modulators in a
defined order and pack/unpack the data for you.

The order is defined by a **custom property** you add to each modulator (any name — e.g. `dumpIndex`)
holding a **non-negative integer** (`0`, `1`, `2`, …). Modulators without the property are skipped;
the block is sized to `(highestIndex + 1) × bytesPerValue`.

### Encoding types

Both methods take a `byteEncoding` telling CtrlrX how each modulator value maps to bytes. Access them
as `CtrlrPanel.<name>`:

| Encoding | Layout |
|---|---|
| `EncodeNormal` | One 7-bit byte (0–127). |
| `EncodeMSBFirst` *(alias `Encode7bitMSBFirst`)* | Two 7-bit bytes, MSB then LSB. |
| `EncodeLSBFirst` *(alias `Encode7bitLSBFirst`)* | Two 7-bit bytes, LSB then MSB. |
| `EncodeNibbleMsbFirst` *(alias `Encode4bitMsbFirst` / `EncodeMsbFirst`)* | Two 4-bit nibbles, MSB nibble first (unsigned). |
| `EncodeNibbleLsbFirst` *(alias `Encode4bitLsbFirst` / `EncodeLsbFirst`)* | Two 4-bit nibbles, LSB nibble first (unsigned). |
| `EncodeSignedNibbleMsbFirst` | Two 4-bit nibbles, MSB first, value treated as signed int8. |
| `EncodeSignedNibbleLsbFirst` | Two 4-bit nibbles, LSB first, value treated as signed int8. |
| `Encode16bitLsbFirst` | A 16-bit value as four 4-bit nibbles, least-significant first. |
| `Encode16bitMsbFirst` | A 16-bit value as four 4-bit nibbles, most-significant first. |

The `bytesPerValue` argument must match the encoding (1 for `EncodeNormal`, 2 for the two-byte /
nibble-pair encodings, etc.).

> 💡 Tip: The **last argument, `useMappedValues`**, chooses which value gets serialized — `false` uses
> the raw modulator value, `true` uses the [mapped value](05-value-mapping.md). Use whichever matches
> what your device expects in the dump.

### Step 1 — tag the modulators (once)

Decide the dump order and set the custom index on each modulator. You can do this from the
[Lua console](09-debugging.md) once, and it's saved with the panel:

```lua
local dumpOrder = { "lfoDelay", "lfoRate", "vcfResonance", "vcfCutoff", "delay" }
for i, name in ipairs(dumpOrder) do
    -- store 0-based index as a string property named "dumpIndex"
    panel:getModulatorByName(name):setProperty("dumpIndex", tostring(i - 1), false)
end
```

To undo, remove the property again: `mod:removeProperty("dumpIndex")`.

### Step 2 — send the dump

```lua
HEADER = "F0 41 00 00 11"   -- your device's dump header
EOX    = "F7"

local data = panel:getModulatorValuesAsData("dumpIndex", CtrlrPanel.EncodeNormal, 1, false)
panel:sendMidiMessageNow(
    CtrlrMidiMessage(string.format("%s %s %s", HEADER, data:toHexString(1), EOX)))
```

`getModulatorValuesAsData` returns a `MemoryBlock`; `toHexString(1)` renders it as space-separated
hex to splice between your header and `F7`. (Add a checksum byte if your device needs one.)

### Step 3 — receive a dump

Assign this to the panel's **midi received** callback (`luaPanelMidiReceived`). Pass the **header
size as a negative number** so CtrlrX skips those header bytes and starts filling modulator index 0:

```lua
panelMidiReceived = function(midi)
    local headerSize = MemoryBlock(HEADER):getSize()
    panel:setModulatorValuesFromData(midi:getData(), "dumpIndex",
                                     CtrlrPanel.EncodeNormal, -headerSize, 1, false)
end
```

> ⚠️ Gotcha: the `propertyOffset` argument is signed. A **negative** value is a *header byte count*
> (skip that many bytes, modulators start at index 0); a **positive** value instead means "data starts
> at byte 0, but modulator indices start at this offset". For skipping a SysEx header you almost always
> want the negative form.

> 🔗 Deeper: both methods and their overloads (including a start/end-index variant) are in the
> [Lua reference](lua/02-lua-reference.md#bulk-modulator-data). See also the
> [SysEx Token List](https://github.com/damiensellier/CtrlrX/wiki/SysEx-Token-List) wiki page for the
> per-parameter token approach when you'd rather not script.

## Keeping a patch image (shadow state)

> **TL;DR** — The bulk methods treat *your modulators* as the source of truth. Sometimes it's easier
> to keep the device's full patch as one **`MemoryBlock`** ("the image"), patch a byte or two into it
> from a callback, and send it (or a region) — handy when the patch has bytes with no on-screen
> control, or when the device only accepts whole-patch/regional writes.

A `MemoryBlock` is a general-purpose byte buffer, not just a bulk-dump helper. You can hold the whole
patch as one, mutate part of it in place, and ship it. The pieces you'll use:

| Do this | With |
|---|---|
| Make a fixed-size, zeroed image | `MemoryBlock(size, true)` |
| Change one byte | `block:setByte(offset, value)` |
| Change a packed bit-field | `block:setBitRange(startBit, numBits, value)` |
| Replace a region from another block | `block:copyFrom(src, destOffset, n)` / `block:replaceWith(src)` |
| Take a slice | `block:getRange(start, n)` |
| Render to hex for a SysEx frame | `block:toHexString(1)` |

### Two models, side by side

| | **Values live in modulators** | **Shadow patch image** |
|---|---|---|
| Source of truth | The modulators (see [bulk dumps](#bulk-dumps--many-modulators-in-one-message)) | A `MemoryBlock` you keep |
| Rebuild the dump | `getModulatorValuesAsData(...)` on demand | already assembled |
| Best when | every byte maps 1:1 to a control | the patch has name strings, reserved/flag bytes, or the device wants whole-patch/region writes |

### Recipe

```lua
-- 1. In the panel "loaded" callback, create the image once, at full size:
patch = MemoryBlock(64, true)                  -- 64 zeroed bytes

-- 2. On a control change (luaModulatorValueChange), patch just that byte and
--    send a single-parameter SysEx carrying its offset + value:
onKnob = function(mod, value)
    local off = tonumber(mod:getProperty("dumpIndex"))
    patch:setByte(off, value)
    panel:sendMidiMessageNow(CtrlrMidiMessage(
        string.format("F0 7D 10 %02X %02X F7", off, value)))
end

-- 3. On an incoming full dump (luaPanelMidiReceived), store the body as the new
--    image and mirror it onto the modulators:
onDump = function(midi)
    local data = midi:getData()
    patch:replaceWith(data:getRange(5, tonumber(data:getSize()) - 6))   -- strip 5-byte header + F7
    panel:setModulatorValuesFromData(data, "dumpIndex", CtrlrPanel.EncodeNormal, -5, 1, false)
end

-- 4. Send the whole image back to the device:
sendAll = function()
    panel:sendMidiMessageNow(CtrlrMidiMessage(
        string.format("F0 7D 00 %s F7", patch:toHexString(1))))
end
```

### Where the image lives

The image is a plain Lua value, so give it a home:

- **For the session** — a **global / module-level** variable (like `patch` above), created in the
  panel-loaded callback. It persists across callbacks as long as the panel is open.
- **Saved with the panel** — serialize it into the panel's save-state `ValueTree` (a `MemoryBlock`
  won't fit in a panel *global variable*, which only stores integers):
  ```lua
  luaPanelSaveState    = function(s) s:setProperty("patch", patch:toBase64Encoding(), nil) end
  luaPanelRestoreState = function(s)
      local b64 = s:getProperty("patch")
      if b64 then patch = MemoryBlock(); patch:fromBase64Encoding(b64) end
  end
  ```

> ⚠️ Gotcha: `getByte`/`setByte` are lenient about bounds — an out-of-range `getByte` returns `0`
> silently instead of erroring. Allocate the image to its full size up front (`MemoryBlock(size,
> true)`) and treat it as fixed-length so a bad offset doesn't pass unnoticed.

> 🔗 Deeper: the full `MemoryBlock` method set (byte/bit/region edits, base64, zlib/gzip, Lua-table
> conversion) is in the [Lua reference](lua/02-lua-reference.md#memoryblock); the save/restore state
> callbacks are covered in [Chapter 10](10-distribution.md#global-variables--persistent-state).

## Back to the example panel

Wire `filterCutoff` to **CC 74 / ch 1** and `filterOn` to **CC 75 / ch 1** (max value 1). Open the
MIDI Monitor, switch to Panel mode, and confirm both send. If you have a virtual Input loopback, send
CC 74 back in and watch the knob move on its own. Save — that's the finished
[example/first-panel.panel](example/first-panel.panel):

![The finished example panel in Panel mode](images/first-panel-final.png)

---

[← 07 Making Responsive](07-making-responsive.md) | [Index](README.md) | Next: [09 — Debugging →](09-debugging.md)
