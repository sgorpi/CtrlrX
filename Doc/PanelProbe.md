# CtrlrPanelProbe — drive a panel from a script, observe what it does

`CtrlrPanelProbe` loads an arbitrary CtrlrX panel, runs a JSON action script against it, and
prints a JSON report of everything it observed, such as MIDI sent to the device, MIDI sent to the host,
errors, and a final snapshot of every modulator. It needs **no hardware and no display**: the
panel's MIDI devices are transparently rebound to the mocked test devices used by the GoogleTest
suite, and it drives the real `CtrlrProcessor` the same way the tests do.

`CtrlrPanelProbe` closes the gap between *"the panel loads"* and *"the panel behaves"*. Dead button
wiring, a control that clamps silently, a `luaModulatorValueChange` that never fires, a SysEx
formula that emits the wrong bytes — all pass lint and load fine, and otherwise could only be
caught by a human clicking things in front of hardware. This tool makes that behaviour observable
from a script.

The probe **reports observations; it does not assert.** The caller (an AI agent session, a shell
script, CI) decides what is correct by reading the report.

## Quick start

```bash
# 1. Build it (or download it, see Getting the binary below).
cmake --build build --target CtrlrPanelProbe -j `nproc`

# 2. What is in this panel? No script = load, rebind, snapshot. Do this FIRST:
#    it is where correct modulator names come from.
./build/Tests/CtrlrPanelProbe MySynth.panel | jq '.finalState.modulators[].name'

# 3. Drive it, and look at what came out.
cat > /tmp/check.json <<'JSON'
{ "steps": [ { "action": "setValue", "modulator": "cutoff", "value": 64 } ] }
JSON
./build/Tests/CtrlrPanelProbe MySynth.panel /tmp/check.json \
  | jq '{ summary: .summary, midi: .steps[0].deviceMidiOut }'
```

Five rules that account for most mistakes, in the order they usually bite:

1. **Modulator and method names are matched exactly**, against the modulator's `name` property.
   Get them from step 2 above rather than guessing. A wrong name is a recorded error of type
   `action`, not a crash — the run continues and exits 1.
2. **Exit code 0 means "nothing raised an error"**, not "the panel did what you wanted". Whether
   the right MIDI came out is yours to check, by reading `steps[].deviceMidiOut`.
3. **Assert visibility on `effectiveVisible`**, never on `visible`. See
   [Visibility](#visibility) — they answer different questions and disagree often.
4. **An absent key is not `null`.** `"returnValue": null` means the Lua call returned `nil`;
   *no* `returnValue` key means the action never produces a Lua value. See
   [Which keys are always present](#which-keys-are-always-present).
5. **Do not raise `quietMs` to wait for something.** Use `waitFor` (a polled condition) or
   `wait` (a fixed floor). See [Settling](#settling).

## Getting the binary

**Build it** alongside the tests. It is an ordinary target of the main CMake project (see
[04 — Compilation Guide](readme/04-compiling.md)):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
cmake --build build -j `nproc`

# or just the probe:
cmake --build build --target CtrlrPanelProbe -j `nproc`
```

The binary lands in `build/Tests/CtrlrPanelProbe` (`CtrlrPanelProbe.exe` on Windows; with a
multi-config generator such as Visual Studio or Xcode, under `build/Tests/<Config>/`).

It is built from the same per-platform mock MIDI layer as the GoogleTest suite
(`Tests/mock_MidiDevice*.cpp`: ALSA on Linux, CoreMIDI on macOS, Win32 on Windows) and the target
itself is not platform-gated.

**Or download it.** Every CI run of
[build_and_test.yml](../.github/workflows/build_and_test.yml) uploads a Release build per platform
as the workflow artifact `CtrlrPanelProbe-Linux`, `CtrlrPanelProbe-macOS` or
`CtrlrPanelProbe-Windows` (on the run's *Summary* page, under *Artifacts*). Inside is a zip with the
binary, this file and the licence files. The binary is not code-signed: on macOS clear the
quarantine flag before the first run (`xattr -d com.apple.quarantine CtrlrPanelProbe`).

## Invocation

```bash
CtrlrPanelProbe [--verbose] [--global-console] [--stdin] <panel-file> [<script-file>]
```

- `<panel-file>` — a `.panel` (or `.bpanelz`) path, resolved relative to the current directory.
  Required. If it does not exist the probe exits 2 without loading anything.
- `<script-file>` — optional JSON action script, also resolved relative to the current directory.
  With no script the probe just loads, rebinds, and snapshots the panel (useful to answer
  *"does it load, and what are the initial values?"*). Naming a script file that does not exist is
  fatal (exit 2); an existing but **empty** file is treated as "no script".
- `--verbose` — show CtrlrX / JUCE / Lua's stderr chatter. **Off by default:** stderr is silenced
  so stdout is the sole, clean JSON contract. Turn it on to debug a panel (you then see the
  `GUI_TRACE` and other `_DBG` logging). It does not change the JSON on stdout.
- `--global-console` — add a top-level `console` array to the report aggregating **every**
  `console()` line the script produced. Opt-in because a chatty panel can emit a lot; the per-step
  `steps[i].console` arrays are always present regardless.
- `--stdin` — persistent mode; see [below](#persistent-mode---stdin).

The **JSON report is written to stdout**; all of CtrlrX's own logging goes to stderr, so stdout is
always clean, parseable JSON (or a JSON error object on a fatal failure).

Argument parsing is deliberately simple, with one trap: **an unrecognised argument starting with
`-` is silently ignored.** A misspelled `--verbse` does not warn, it just does nothing. Anything
not starting with `-` is positional; extra positionals beyond the first two are ignored.

## Script schema

A script is one JSON object. Everything in it is optional — `{}` is a valid script that behaves
like passing no script at all.

```json
{
  "settleMs": 300,
  "quietMs": 30,
  "steps": [
    { "action": "setValue", "modulator": "pitchCents", "value": 1200 },
    { "action": "click", "modulator": "fetchBtn" },
    { "action": "injectDeviceMidi", "bytes": "F0 42 30 00 04 F7" },
    { "action": "injectHostMidi", "bytes": "B0 07 40" },
    { "action": "callLuaMethod", "method": "refreshAll" },
    { "action": "eval", "lua": "return codec.get(img, 'filters.speed')" },
    { "action": "waitFor", "lua": "return dump_done", "timeoutMs": 2000 },
    { "action": "screenshot", "file": "panel.png" },
    { "action": "wait", "ms": 500 }
  ]
}
```

Steps run in order, in one process, against one panel. **State accumulates** — step 3 sees
whatever steps 1 and 2 did.

### Settling

After every action the probe pumps the message loop and runs `processBlock`s in ~5 ms slices,
draining the MIDI recorders as it goes, so async modulator callbacks, the panel's MIDI-out thread
and the host output all flush before the step is recorded.

It stops as soon as **nothing new has happened for `quietMs`** — no device send, no host MIDI, no
log line (any log line, not just `console()`) — and gives up at the `settleMs` ceiling.

- `settleMs` (default `300`) is a **ceiling, not a floor**. A step that does nothing asynchronous
  costs about `quietMs`, not `settleMs`.
- `quietMs` (default `30`) is the quiet window.
- Both are settable script-wide **and per step** — every action object accepts them, so one slow
  step can be given more room without slowing the whole script.
- `settleMs: 0` still pumps once and runs one `processBlock`. It does not skip settling entirely,
  because `CtrlrLog` is an `AsyncUpdater`: a hard bypass would silently drop `console()` output and
  Lua error text.
- A panel with a chatty repeating timer never goes quiet, so it simply costs the ceiling.

If a step is waiting on something the probe cannot see as activity, use `wait` (a floor) or
`waitFor` (a polled condition) rather than raising `quietMs`. Raising `quietMs` slows down *every*
step and still gives up at `settleMs`.

### Actions

Every action object needs an `"action"` key. Any other key is specific to that action, except
`settleMs` / `quietMs`, which all of them accept.

| Action            | Required            | Optional              | Effect |
|-------------------|---------------------|-----------------------|--------|
| `setValue`        | `modulator`, `value`|                       | Sets the modulator via the **GUI path** (`setComponentValue` → `changedByGUI`), so the full knob-move wiring fires: MIDI is sent and `luaModulatorValueChange` runs. `value` is a number in the modulator's own units (the `min`…`max` reported in `finalState`), not normalised. |
| `click`           | `modulator`         |                       | Drives the real component mouse-up path: `mouseDown` → (`triggerClick`) → `mouseUp`, so `componentLuaMouseDown` / `componentLuaMouseUp` **and** the value-change / MIDI path all fire. A `uiButtonTriggerOnMouseDown` button self-triggers on the down event (no double click). |
| `injectDeviceMidi`| `bytes`             |                       | Injects a MIDI message as if it arrived from the (mock) hardware input; fires the panel's input comparator + `luaPanelMidiReceived`. |
| `injectHostMidi`  | `bytes`             |                       | Carries a MIDI message into the next `processBlock` as host input. |
| `callLuaMethod`   | `method`            | `args`                | Calls a named Lua method and reports its return value in `steps[i].returnValue`. |
| `eval`            | `lua`               |                       | Runs an arbitrary Lua chunk in the panel's own `lua_State`; result in `steps[i].returnValue`. |
| `waitFor`         | `lua`               | `timeoutMs`, `pollMs` | Polls a Lua condition until it is truthy. |
| `screenshot`      | `file`              |                       | Renders the live canvas to a PNG. |
| `setLayerVisible` | `layer`, `visible`  |                       | Sets a canvas layer's visibility (layers are pages). `layer` matches by **uid first, then name**. Sets the `uiPanelCanvasLayerVisibility` tree property, which the report reads back. |
| `wait`            |                     | `ms`                  | Pumps for exactly `ms` (default: the step's effective `settleMs`) with **no early exit** — useful to observe delayed/queued output. |

`bytes` is whitespace- or comma-separated hex (`"F0 42 30 F7"`, `"b0,07,40"`, `0x`-prefixes ok).
Unparseable `bytes` is an `action` error, not a crash.

Anything else in `"action"` — a typo, an action from a newer version — is reported as
`unknown action '<name>'` of type `action`, and the script continues.

#### `callLuaMethod`

Panel methods are compiled as Lua globals by name, so the probe fetches the global and calls it
through the raw **Lua 5.1 C API** (`lua_getglobal` → push args → `lua_pcall`), reporting the single
return value in `steps[i].returnValue`.

- This is the path **whether or not `args` is present** — a nullary method's return value is
  readable.
- **`args` must be a JSON array**, one element per Lua argument, even for a single argument:
  `"args": [7]`, not `"args": 7`. A non-array `args` is silently treated as no arguments at all.
- **Arguments may be any JSON value.** Scalars map to Lua `number` / `string` / `boolean` / `nil`;
  JSON arrays become Lua tables keyed `1..n`; JSON objects become string-keyed tables. Nesting is
  capped at depth 8.
- Values are pushed as typed Lua values, **not** concatenated into source — there is no string
  building and therefore no code-injection surface.
- A method that raises is reported as an `errors[]` entry of type `"lua"`; it does **not** pop the
  modal "Callback error" dialog that CtrlrX's `CATCH_METHOD_EXCEPTION` path would.
- A name that is not a known panel method, or one whose method failed to compile, is an `action`
  error (`no Lua method named '…'` / `… is invalid`), not a Lua error.

#### `eval`

```json
{ "action": "eval", "lua": "return codec.get(fw_image, 'filters.speed')" }
```

Runs the chunk in the panel's own `lua_State`, so module tables, globals and anything a panel
method can see are directly assertable — no need to inject a throwaway `<luaMethod>` into a scratch
copy of the panel just to read a value out.

The chunk is tried as an **expression** first (`return <chunk>`), exactly as the stock Lua REPL
does, then verbatim if that does not compile. So all three of these work:

```json
{ "lua": "return someTable.field" }
{ "lua": "someFunction(1, 2)" }
{ "lua": "local t = build() return #t" }
```

Return values convert to JSON: a Lua table becomes a JSON **array** when its keys are exactly
`1..n`, and an **object** otherwise. Nesting is capped at depth 8 (`"<max-depth>"`), cycles are
detected (`"<cycle>"`), and values with no JSON equivalent are reported as `"<function>"` /
`"<userdata>"` / `"<thread>"` rather than silently dropped.

Remember this is JSON: quotes inside the chunk have to be escaped, which is why the examples above
use single quotes for Lua strings.

#### `waitFor`

```json
{ "action": "waitFor", "lua": "return dump_done", "timeoutMs": 2000, "pollMs": 25 }
```

Polls the chunk through the same pump loop as settling until it returns a truthy value. **Lua
truthiness applies**: only `nil` and `false` are false, so `0` and `""` are truthy. Defaults:
`timeoutMs` 2000, `pollMs` 25. `returnValue` carries the last value the condition produced, whether
it succeeded or timed out. On timeout the step gets an `errors[]` entry of type `"action"`; if the
chunk itself raises, that is an error of type `"lua"` and polling stops. `waitFor` does its own
pumping, so no settle runs after it.

Use this for genuinely asynchronous flows instead of guessing a `wait` duration.

#### `screenshot`

```json
{ "action": "screenshot", "file": "out.png" }
```

Renders the live canvas with `createComponentSnapshot` and writes a PNG, reporting
`steps[i].screenshot = { file, width, height }` — where `file` is the **absolute** path the probe
resolved `file` to, relative to the current working directory. The canvas never had a desktop peer
but is fully built and laid out, so this paints what the editor window would. The canvas's
always-on-top `ResizableBorderComponent` is hidden for the duration so it does not frame every
image.

## Report schema

```json
{
  "reportVersion": 1,
  "panel": {
    "file": "/abs/path/to.panel",
    "name": "SysEx Token",
    "originalDevices": { "input": "Device 00, port 00", "output": "Device 00, port 01" },
    "luaCompileErrors": []
  },
  "steps": [
    {
      "index": 0,
      "action": { "action": "setValue", "modulator": "test-modulator-1", "value": 64 },
      "deviceMidiOut": ["b0 0c 40", "b0 0d 41", "b0 0e 42", "f0 7e 41 f7"],
      "hostMidiOut":   ["b0 0c 40"],
      "errors": [],
      "console": ["set to 64"],
      "ok": true
    }
  ],
  "finalState": {
    "modulators": [
      { "name": "test-modulator-1", "value": 64.0, "componentType": "uiSlider", "text": "64",
        "visible": true, "componentRectangle": "16 16 72 96",
        "componentLayerUid": "2a4c…", "liveVisible": true, "effectiveVisible": true,
        "canvasRectangle": "16 16 72 96", "vstIndex": 1, "min": 0.0, "max": 127.0 }
    ],
    "layers": [
      { "name": "New layer", "visible": true, "uid": "2a4c…" }
    ],
    "overlaps": [
      { "a": "change-count", "b": "last-value", "rect": "150 40 38 72" }
    ]
  },
  "summary": {
    "errorCount": 0,
    "actionErrorCount": 0,
    "luaRuntimeErrorCount": 0,
    "luaCompileErrorCount": 0,
    "exitCode": 0
  }
}
```

- `panel.originalDevices` — the MIDI device names the panel carried **before** rebinding (often
  `"-- None"`). The file on disk is never modified; rebinding happens in memory only.
- `panel.luaCompileErrors` — an array of `"method 'name': <error>"` strings for methods that failed
  to compile at load, captured **before any step runs** (so a runtime error later cannot be
  misreported here).
- Per step: `deviceMidiOut` / `hostMidiOut` are lowercase-hex byte strings captured during that
  step's settle; `console` is that step's `console()` output; `ok` is `errors` being empty.
- `finalState` is a snapshot taken once, **after the last step**. There is no per-step state
  snapshot; to see a value mid-script, `eval` it or read the MIDI it emitted.

### Which keys are always present

Do not test for a key by comparing it to `null` — check whether it exists.

| Key | Present |
|-----|---------|
| `reportVersion`, `panel`, `steps`, `finalState`, `summary` | always (on a non-fatal run) |
| `console` (top level) | only with `--global-console` |
| `steps[i].index` / `action` / `deviceMidiOut` / `hostMidiOut` / `errors` / `console` / `ok` | always |
| `steps[i].returnValue` | only for `callLuaMethod`, `eval`, `waitFor` — and its value is `null` when Lua returned `nil` |
| `steps[i].screenshot` | only for a `screenshot` action that wrote a file |
| modulator `liveVisible` / `effectiveVisible` | only when the modulator has a live component |
| modulator `canvasRectangle` | only when the live component also has a parent (see the layer-uid gotcha) |

### Errors

There is **one** error channel. `steps[i].errors` is an array of `{ type, message }`:

| `type`     | Meaning |
|------------|---------|
| `"action"` | The probe could not carry out what the step asked — unknown action, no such modulator or method, unparseable `bytes`, a `waitFor` timeout. |
| `"lua"`    | The panel's Lua raised — a method that threw, an `eval` chunk that failed, a callback CtrlrX marked invalid. |

Compile errors are the third channel and live outside the steps, in `panel.luaCompileErrors`.

`summary.errorCount` is the **total of all three counted channels** (`actionErrorCount` +
`luaRuntimeErrorCount` + `luaCompileErrorCount`). Asserting `summary.errorCount == 0` (or on the
exit code) is sufficient; you cannot get a green result by checking a channel that happened to be
empty while another was not.

### Visibility

Three fields, because they answer three different questions:

| Field              | Source | Use it for |
|--------------------|--------|------------|
| `visible`          | the `componentVisibility` **tree property** | what the panel *document* says; present even for a component that was never realized |
| `liveVisible`      | the live `Component::isVisible()` | catches a Lua `comp:setVisible(false)`, which never writes the tree |
| `effectiveVisible` | `liveVisible` **and** every ancestor up to the canvas | **assert on this one** — it folds in a hidden group, layer or page |

`liveVisible` / `effectiveVisible` are omitted when the modulator has no live component;
`canvasRectangle` additionally needs that component to have a parent (see the fixture gotcha below).

### Geometry and overlaps

- `componentRectangle` is `getBoundsInParent()` as `"x y w h"` — for a control inside a `uiGroup`
  this is **group-relative**.
- `canvasRectangle` is the same bounds converted to absolute, canvas-relative coordinates.
- `finalState.overlaps` lists every pair of **effectively visible** components whose
  `canvasRectangle`s intersect, as `{ a, b, rect }`, skipping ancestor/descendant pairs (a group
  legitimately contains its own controls). A component with no live parent takes no part in it.

Overlap detection has to work off the live component tree for exactly this reason: intersecting the
reported `componentRectangle`s would compare a group-relative rectangle against a canvas-relative
one, both missing real overlaps and inventing false ones. This is the check a panel generator would
otherwise hand-roll.

## Persistent mode (`--stdin`)

```bash
CtrlrPanelProbe --stdin MySynth.panel < scripts.jsonl > reports.jsonl
```

Reads **one JSON script object per input line** and writes **one compact JSON report object per
output line** (the same schema, printed on a single line instead of pretty-printed). Blank lines
are skipped. EOF exits 0.

This exists to amortise the ~1.5 s of process startup (JUCE init, async MIDI device enumeration,
panel parse) across a whole suite. Measured on a 1.35 MB, 313-modulator panel:

| | time | peak RSS |
|---|---|---|
| one-shot process, per script | 1.50 s | 62 MB |
| `--stdin`, 50 scripts | 1.8 s total (~6 ms/script) | 62 MB |

**Scripts in one process share panel state** — modulator values, Lua globals, everything. There is
deliberately no in-process `reload`: it was built and measured at ~1.74 s per reload (0.6–1.1 s of
teardown through `CtrlrManager::removePanel`'s UI-flavoured path, plus ~1.0 s to re-open) with
~0.8 MB of RSS growth each time, making it both slower than simply respawning the process and
leakier. **Isolation means respawning.**

The practical pattern, e.g. in pytest: a module-scoped fixture holding one `--stdin` process for
the many tests that only read or that tolerate shared state, and a plain per-test `subprocess.run`
for the few that need a pristine panel.

Two things to know about the reports this mode emits:

- `panel.luaCompileErrors` is the **load-time baseline**, so it is repeated identically in every
  report on the stream — and therefore so is its contribution to `summary.errorCount`. A panel with
  one broken method never reports `errorCount == 0` for any script.
- A malformed input line yields `{"reportVersion": 1, "error": "script parse error", "detail": …}`
  on its own line and the loop continues — it is the client's bug, not a broken panel. Note this
  object has no `summary`, so a consumer must check for `error` before reading `summary`.

## Exit codes

| Code | Meaning |
|------|---------|
| `0`  | Clean run (and, in `--stdin` mode, EOF reached). |
| `1`  | `summary.errorCount > 0`. The full report is still printed. |
| `2`  | Fatal: the panel file was missing or failed to load, or the script file was missing/malformed. |

On a fatal failure stdout carries a minimal object instead of a report:
`{ "reportVersion": 1, "error": "<what>", "detail": "<which>", "exitCode": 2 }`. This happens
before any script runs, so it applies to `--stdin` mode too.

In `--stdin` mode the exit code otherwise reflects the process, not any one script; read each
report's `summary.errorCount`.

## Worked example

Verify that dragging a "pitch cents" slider emits the expected pitch-bend SysEx, that a "fetch"
button actually sends its request, and that a name label updated — all without hardware:

```bash
cat > check.json <<'EOF'
{ "steps": [
  { "action": "setValue", "modulator": "pitchCents", "value": 1200 },
  { "action": "click",    "modulator": "fetchBtn" }
] }
EOF

CtrlrPanelProbe MySynth.panel check.json | jq '{
  errors: .summary.errorCount,
  bend:   .steps[0].deviceMidiOut,
  fetch:  .steps[1].deviceMidiOut,
  name:   (.finalState.modulators[] | select(.name=="nameLabel") | .text),
  laps:   .finalState.overlaps
}'
```

If `steps[0].deviceMidiOut` is empty, the slider's MIDI wiring is dead. If `steps[1].deviceMidiOut`
is empty, the button callback never fired. To read a value straight out of the panel's Lua rather
than inferring it from a label:

```bash
echo '{"steps":[{"action":"eval","lua":"return codec.get(img, \"filters.speed\")"}]}' > e.json
CtrlrPanelProbe MySynth.panel e.json | jq '.steps[0].returnValue, .summary.errorCount'
```

## Notes / gotchas

- **Device rebind** targets the mock devices `"Device 00, port 00"` (in) / `"Device 00, port 01"`
  (out). Any panel's I/O is captured regardless of the device names it shipped with.
- **Modal dialogs are suppressed.** CtrlrX pops a blocking modal "Callback error" AlertWindow on a
  runtime Lua error raised from a *callback*; the probe auto-dismisses it so it never hangs (on a
  display or on headless CI). The error is still recorded in the step's `errors`. Errors from
  `callLuaMethod` / `eval` come back through `lua_pcall` and never reach that path at all.
- **Hard exit.** The probe prints its report(s) and calls `_Exit()` without tearing down JUCE — the
  panel/Lua object graph is known to leak on shutdown and the Debug leak detector would otherwise
  fire after the report. This is deliberate.
- **A component only gets a parent if its `componentLayerUid` names a real layer.**
  `CtrlrPanelCanvas::addAndMakeVisibleNg` silently leaves a component unparented when its
  `componentLayerUid` matches no `uiPanelCanvasLayerUid`. Such a component still reports
  `liveVisible` / `effectiveVisible` (it exists, it just has no parent), but `canvasRectangle` is
  absent and it never appears in `overlaps`. Several older `Tests/fixtures/fixture_*.panel` files
  have this defect (each modulator carries its own bogus uid); real panels saved by CtrlrX do not. If
  `canvasRectangle` is missing where you expect it, check that first — compare
  `finalState.modulators[].componentLayerUid` against `finalState.layers[].uid`.
- **Device→host MIDI forwarding lags a step.** A message injected with `injectDeviceMidi` updates
  the modulator within the step, but the copy forwarded to the host tends to appear in the *next*
  step's `hostMidiOut`. This is a `MidiMessageCollector` timing property, not something `quietMs`
  can fix. Add a `wait` step if you need to observe it.
- **Load-ordering caveat (`panelLoaded`).** Components are built as each modulator restores, so a
  `luaPanelLoaded` callback that calls `setVisible` on a control *before that control has finished
  restoring* is a no-op against a not-yet-existing component. Drive visibility from a value-change
  or nav callback (as the fixtures do) rather than at load time.
- **A Debug build writes `~/Desktop/CtrlrX_DBG.log`.** `ctrlrLogToFile` is forced on in
  `JUCE_DEBUG` builds, and JUCE's `FileLogger` only trims that file when it is constructed — so a
  long `--stdin` session, or a suite spawning many probes, appends to it without bound. Watch it if
  you are driving a chatty panel hard.
- **Self-test:** `ctest -R PanelProbeSelfTest` (see [probe_selftest.py](../Tests/probe_selftest.py)) —
  16 scenarios, most of which pin a specific defect so it cannot come back silently. Run it after
  changing `probe_*.cpp`; it is also the best worked reference for what each report field looks
  like in practice.
