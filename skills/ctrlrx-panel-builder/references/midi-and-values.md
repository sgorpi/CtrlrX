# MIDI Messages & Value Mapping

Everything here is set on a modulator's `<midi>` child and on the modulator's value attributes.
When the source disagrees with this page, the source wins — see `Source/MIDI/CtrlrMidiMessage.*`
and `Source/Core/CtrlrSysexProcessor.*` / `CtrlrSysexToken.*`.

## `midiMessageType` — the integer enum

The `<midi midiMessageType="N">` attribute is an **integer**, not a name:

| N | Type | Wire bytes | Typical use |
|---|---|---|---|
| 0 | CC | `Bn cc vv` | The workhorse — most parameters. |
| 1 | Aftertouch (poly) | `An nn vv` | Per-note pressure. |
| 2 | ChannelPressure | `Dn vv` | Channel aftertouch. |
| 3 | NoteOn | `9n nn vv` | Trigger a note/pad. |
| 4 | NoteOff | `8n nn vv` | Release a note. |
| 5 | SysEx | `F0 … F7` | Manufacturer params via a formula (below). |
| 6 | Multi | several messages | 14-bit, NRPN/RPN, or any sequence (below). |
| 7 | ProgramChange | `Cn pp` | Select a patch/program. |
| 8 | PitchWheel | `En ll mm` | 14-bit bend — set max value `16383`. |
| 9 | None | — | No MIDI (Lua-only or display control). |
| 10–14 | MidiClock / …Continue / …Stop / …Start / ActiveSense | realtime | Clock / transport. |

(`n` = channel nibble, channel 1 → `0`.) For a simple control, set `midiMessageType`,
`midiMessageChannel` (1–16), and `midiMessageCtrlrNumber` (the CC/note/program number). For a switch,
set `modulatorMax="1"`.

## Raw range and the value attributes

A control's raw value lives between `modulatorMin` and `modulatorMax` (on `<modulator>`), and the
current value is `modulatorValue`. Match the max to the MIDI resolution:

- 7-bit CC / note / program → `0`..`127`
- 14-bit (PitchWheel, or 14-bit CC / NRPN) → `0`..`16383`
- on/off switch → `0`..`1`

> ⚠️ Gotcha — **the value byte comes from the modulator's value, never from
> `midiMessageCtrlrValue`.** For CC, NoteOn, NoteOff, Aftertouch, ChannelPressure and PitchWheel,
> CtrlrX builds the message as `…(ch, getNumber(), getValue())`, where `getValue()` is the
> modulator's *current value* (`CtrlrMidiMessage.cpp:1004-1019`). The `midiMessageCtrlrValue`
> attribute is not consulted on that path despite its name. Two consequences that bite:
> - A trigger button with `modulatorMin/Max` of `0..1` emits **velocity 0 or 1**, whatever
>   `midiMessageCtrlrValue` says. Velocity 0 *is* a NoteOff by MIDI convention, so the device
>   typically ignores it entirely and the button appears dead. Widening `modulatorMax` does not
>   help either if the component's value never reaches it — see the `uiButton` valueMap trap in
>   `components.md`.
> - To send a **fixed** value byte (a set velocity, a constant CC), set `midiMessageType="9"`
>   (None) and emit it from Lua: `panel:sendMidiMessageNow("9F 36 64")`.

> ⚠️ Gotcha — **a PitchWheel modulator left at `0..127` silently throws away 99.2% of its range.**
> Type 8 emits a 14-bit value as `En ll mm`, so `modulatorMax="127"` maps full travel onto
> 14-bit 0..127 — the very bottom of the bend range (`ef 7f 00`), and inbound centre (`EF 00 40`,
> 8192) clamps to the top of the control. Set **both** `modulatorMax="16383"` *and* the component's
> `uiSliderMax="16383"` (the visible range is a separate property — see `components.md`).
> Centre is `8192` (`ef 00 40`). Note that a 16384-step range on a small knob needs
> `uiSliderMouseWheelInterval` raised, or fine adjustment is unreachable by mouse.

## Multi messages (14-bit, NRPN/RPN, sequences)

Set `midiMessageType="6"` and fill `midiMessageMultiList`. The string is a sequence of messages
separated by **colons** `:`; each message is comma-separated:

```
Type[,Number[,Value]] : Type[,Number[,Value]] : …
```

For **Number** and **Value** you can use a literal or a token:

| Token | Means |
|---|---|
| `Direct`, `Default`, `Value`, or `-1` | The modulator's current value. |
| `Number`, `CtrlNumber`, or `-2` | The modulator's MIDI number. |
| a decimal, e.g. `74` | A literal number. |
| a 1–2 digit hex written bare, e.g. `40` (= 0x40) | A literal hex byte. |

**14-bit CC** (0–16383 across CC 1 MSB + CC 33 LSB) — set `modulatorMax="16383"`:
```
CC,1,Direct : CC,33,Direct
```
CtrlrX splits the high/low 7 bits across the two messages when the value exceeds 127.

**NRPN** (select param with CC 99/98, set with CC 6 [+38]):
```
CC,99,2 : CC,98,10 : CC,6,Direct
```
(Parameter 2/10, data = the modulator value.)

**Fixed sequence** (e.g. a panic button, type `None` on the modulator, triggered from Lua/Multi):
```
CC,123,0 : CC,120,0
```

> Legacy form: older panels use a longer token form (`ByteValue`, `MSB7bitValue`, `LSB7bitValue`,
> e.g. `CC,ByteValue,MSB7bitValue,28,-1:…`). CtrlrX still understands it (routed through the SysEx
> engine), but prefer the short `Type,Number,Value` form for new panels.

## SysEx messages (formula & tokens)

Set `midiMessageType="5"` and write `midiMessageSysExFormula`: the raw bytes of the message as
two-character literal **hex**, with short **token codes** (not long names) where dynamic data goes.
The formula string is space-separated; each entry is either a hex byte or one of these codes, mapped
by `CtrlrSysexProcessor::sysExIdentifyToken` (`Source/Core/CtrlrSysexProcessor.cpp`):

| Code | Inserts | Enum |
|---|---|---|
| `xx` | Modulator value as one 7-bit byte | `ByteValue` |
| `MS` / `LS` | Value high / low 7 bits (values > 127) | `MSB7bitValue` / `LSB7bitValue` |
| `ms` / `ls` | Value high / low 4-bit nibble | `MSB4bitValue` / `LSB4bitValue` |
| `yy` | MIDI channel (7-bit); `0y` = channel in low nibble (4-bit) | `ByteChannel` / `ByteChannel4Bit` |
| `r1`–`r4` | Roland address/value byte splits | `RolandSplitByte1..4` |
| `q0`–`q3` / `Q0`–`Q3` | 16-bit value as 4-bit nibbles, LSB / MSB first | `Nibble16bitLsb/Msb0..3` |
| `tp` / `tb` | Current program / bank number | `CurrentProgram` / `CurrentBank` |
| `k…` `o…` `p…` `n…` | Panel global variable (hex digit selects which) | `GlobalVariable` |
| `Xn` `zn` `wn` `On` `tc` | Checksum over the *n* preceding bytes — XOR, 2's-complement (Roland/Yamaha), simple sum, 1's-complement, Technics | `Checksum*` |
| `ii` | Wildcard / ignore this byte on incoming match | `Ignore` |
| `f0` / `f7` | Literal `F0` / `F7` | — |
| `u…` / `v…` | Byte computed by Lua / a sub-formula | `LUAToken` / `FormulaToken` |

> ⚠️ These short codes are the SysEx-**formula** vocabulary. They are **different** from the *Multi
> list* legacy long names (`ByteValue`, `MSB7bitValue`, `LSB7bitValue`) parsed by `indirectFromString`.
> In a SysEx formula write `xx`, not `ByteValue`.

The digit after a checksum code is how many preceding bytes it covers:
`startByte = tokenPosition - digit`.

**Example** — manufacturer `7D`, set parameter `0x12` to the knob value, XOR checksum over the 3
bytes before it:
```
F0 7D 12 xx X3 F7
```
On a value change CtrlrX substitutes `xx`, replaces `X3` with `7D ^ 12 ^ value`, and emits the full
`F0 … F7`. Full list on the CtrlrX wiki "SysEx Token List". For devices too complex for the token
language, compute the bytes in Lua and send with `panel:sendMidiMessageNow(...)` (see `lua-api.md`).

## Value expressions (math without Lua)

Each modulator has three expression attributes (default = pass-through). Use them for linear scaling
and bit-twiddling; reach for Lua only for lookup tables or real logic.

| Attribute | Direction | Default |
|---|---|---|
| `modulatorValueExpression` | modulator value → outgoing MIDI value | `modulatorValue` |
| `modulatorValueExpressionReverse` | incoming MIDI value → modulator value | `midiValue` |
| `modulatorControllerExpression` | MIDI-learn controller input → modulator value | `value` |

**Variables**: `modulatorValue`, `modulatorMappedValue`, `midiValue`, `midiNumber`, `vstIndex`, plus
`panel.<prop>` and `global.<n>` scopes.
**Functions**: `abs`, `ceil`, `floor`, `mod`, `fmod`, `pow`, `min`, `max`; comparisons
`eq`/`lt`/`lte`/`gt`/`gte`; bit helpers `setBit`/`isBitSet`/`clearBit`/`getBitRangeAsInt`/
`setBitRangeAsInt`; `setGlobal`; and ordinary `+ - * /`.

**Example** — halve on the way out, double on the way back:
```
modulatorValueExpression       = floor(modulatorValue / 2)
modulatorValueExpressionReverse = midiValue * 2
```

> ⚠️ Expressions shape the **MIDI number** sent/received — they do **not** change what the control
> *displays*. To put a mapped/computed value on a knob's own readout (Hz, a named step, "Off"), use a
> `uiFixedSlider` + value map (see `components.md`); a plain `uiSlider` readout is only
> `number + uiSliderValueSuffix`.

## Value maps (named choices)

A value map turns raw values into named choices (for `uiCombo`, `uiFixedSlider`,
`uiFixedImageSlider`). Conceptually it's an ordered list of *raw value → mapped value → display
text*; the component shows the text, the processor sends the raw value. Set `modulatorMax` to the
highest index.

```
0=Sine
1=Triangle
2=Sawtooth
3=Square
```

The exact XML child-element names for a stored value map vary by build, so **copy a value-map block
from a panel that already has a working combo and edit the entries** rather than synthesizing the
structure. Confirm against `Source/Core/` (`CtrlrValueMap`) if you must build one from scratch.

## Receiving MIDI

Incoming MIDI is matched to a modulator automatically by **message type + channel + number** (and,
for SysEx, by data pattern). A control that *sends* CC 74 ch 1 also *receives* CC 74 ch 1 with no
extra config — its on-screen value follows the hardware. For custom handling (bulk SysEx dumps, one
message updating many controls), use the panel callback `luaPanelMidiReceived` (see `lua-api.md`),
and use the `source` argument on `luaModulatorValueChange` to avoid MIDI feedback loops.

### `Multi` modulators do **not** receive — verified 2026-09-15

The automatic matching above holds for single-message types. **A `Multi` (NRPN/RPN/14-bit)
modulator is never updated by incoming MIDI**, even though the code path looks complete
(`CtrlrMidiInputComparatorMulti::match` → `setValueFromMIDI`). Measured on a shipped 225-NRPN
panel, on CtrlrX's own `Tests/fixtures/fixture_lua_multi_received.panel`, and by CtrlrX's own
test suite: `MidiLua.device_input_invokes_lua_multiMidiReceived_once` **fails**, while
`MidiRouting.device_input_matching_cc_updates_modulator` and
`MidiSysex.sysex_input_updates_modulator` pass in the same run.

Consequences for panel design:

- **Never promise bidirectional sync through NRPN.** If the device must drive the panel, use a
  SysEx bulk dump decoded in `luaPanelMidiReceived`, or plain CC / Program Change — the
  single-message shapes that do match. A `modulatorValueExpressionReverse` on a `Multi`
  modulator is dead code; write the correct one anyway, but do not expect to test it.
- **Testing this under the probe: inject fast.** `match()` arms a **200 ms** timer whose
  callback clears the part-assembled message, so a script that settles between the four CCs of
  an NRPN burst measures the timer, not the comparator. Use `"settleMs": 10` on each
  `injectDeviceMidi` step and settle only on the last one.
- **A negative test here passes vacuously unless you pair it with a control.** Assert
  alongside it that an injected SysEx (or CC) *does* move a modulator, or a broken probe
  binding looks like the same result.

The match table is built once, at load, from `CtrlrPanel::bootstrapPanel` →
`editModeChanged(uiPanelEditMode)`, so a panel saved with `uiPanelEditMode="1"` never builds
one at all — worth checking first if *no* input matches.
