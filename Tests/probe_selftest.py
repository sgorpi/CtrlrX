#!/usr/bin/env python3
"""Self-test for CtrlrPanelProbe (see Doc/PanelProbe.md).

Runs the probe against known fixtures and asserts the JSON report structure, the exit codes
(0 clean / 1 lua-or-step error / 2 fatal), and a set of concrete observations. Invoked from
ctest as: python3 probe_selftest.py <path-to-CtrlrPanelProbe>

Most scenarios here exist to pin a specific defect the probe used to have, so that it cannot
come back silently: a nullary method whose return value was unreadable, non-scalar arguments
that became nil, failures split across two channels so asserting on one passed vacuously, a
control reported visible while it was hidden, and a per-step settle that cost ~450 ms whatever
the step did.

The fixtures and the scripts this drives are copied next to this file in the build dir, so we
chdir here first.
"""

import json
import os
import subprocess
import sys
import tempfile
import time
from typing import NoReturn


def run(probe, *args):
    """Run the probe; return (exit_code, parsed_stdout_json_or_None, raw_stdout)."""
    proc = subprocess.run([probe, *args], capture_output=True, text=True, timeout=90)
    try:
        report = json.loads(proc.stdout)
    except json.JSONDecodeError:
        report = None
    return proc.returncode, report, proc.stdout


def run_script(probe, panel, script):
    """Run an inline script dict against a panel; return (exit_code, report, raw_stdout)."""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(script, f)
        path = f.name
    try:
        return run(probe, panel, path)
    finally:
        os.unlink(path)


def run_stdin(probe, panel, scripts):
    """Feed several script dicts to one --stdin process; return the list of reports."""
    payload = "".join(json.dumps(s) + "\n" for s in scripts)
    proc = subprocess.run([probe, "--stdin", panel], input=payload,
                          capture_output=True, text=True, timeout=90)
    reports = []
    for line in proc.stdout.splitlines():
        if line.strip():
            reports.append(json.loads(line))
    return proc.returncode, reports


def fail(msg) -> NoReturn:
    print("SELFTEST FAIL:", msg, file=sys.stderr)
    sys.exit(1)


def require(cond, msg):
    if not cond:
        fail(msg)


def find_mod(report, name):
    """Return the finalState snapshot of the named modulator, failing if it is absent."""
    for m in report["finalState"]["modulators"]:
        if m["name"] == name:
            return m
    fail(f"no modulator named '{name}' in finalState")


def main():
    if len(sys.argv) < 2:
        fail("usage: probe_selftest.py <path-to-CtrlrPanelProbe>")
    probe = sys.argv[1]

    # Fixtures / scripts are copied next to this script in the build dir.
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    # --- Scenario 1: clean run with a setValue driving an onChange callback (exit 0) ---
    code, rep, raw = run(probe, "fixtures/fixture_lua_valuechange.panel", "probe_selftest.json")
    require(rep is not None, f"scenario 1 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 1 expected exit 0, got {code}")

    # Structure
    for key in ("panel", "steps", "finalState", "summary"):
        require(key in rep, f"scenario 1 report missing top-level key '{key}'")
    require(rep["panel"]["luaCompileErrors"] == [], "scenario 1 expected no compile errors")
    require(len(rep["steps"]) == 1, "scenario 1 expected exactly one step")

    require(rep["reportVersion"] == 1, "scenario 1 report should carry reportVersion 1")

    step = rep["steps"][0]
    for key in ("index", "action", "deviceMidiOut", "hostMidiOut", "errors", "ok"):
        require(key in step, f"scenario 1 step missing key '{key}'")
    require(step["ok"] is True, f"scenario 1 step not ok: {step['errors']}")
    require(step["errors"] == [], f"scenario 1 unexpected errors: {step['errors']}")

    # Observations: the onChange callback fired once with the driven value, via the GUI source (4).
    require(find_mod(rep, "src")["value"] == 40.0, "scenario 1 'src' should read 40")
    require(find_mod(rep, "change-count")["value"] == 1.0,
            "scenario 1 onChange should fire exactly once (change-count == 1)")
    require(find_mod(rep, "last-value")["value"] == 40.0,
            "scenario 1 onChange should receive value 40")
    require(find_mod(rep, "last-source")["value"] == 4.0,
            "scenario 1 onChange source should be changedByGUI (4)")
    require(rep["summary"]["exitCode"] == 0, "scenario 1 summary.exitCode should be 0")

    # --- Scenario 2: panel with an invalid Lua method -> exit 1, compile error reported ---
    code, rep, raw = run(probe, "fixtures/fixture_lua_invalid.panel")
    require(rep is not None, f"scenario 2 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 1, f"scenario 2 expected exit 1, got {code}")
    require(len(rep["panel"]["luaCompileErrors"]) >= 1,
            "scenario 2 should report at least one luaCompileError")
    require(rep["summary"]["exitCode"] == 1, "scenario 2 summary.exitCode should be 1")
    # errorCount is the single number a caller asserts on: it must include compile errors.
    require(rep["summary"]["errorCount"] == rep["summary"]["luaCompileErrorCount"] >= 1,
            f"scenario 2 errorCount should total the compile errors: {rep['summary']}")

    # --- Scenario 3: nonexistent panel -> fatal exit 2 with a JSON error object ---
    code, rep, raw = run(probe, "this_panel_does_not_exist.panel")
    require(rep is not None, f"scenario 3 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 2, f"scenario 3 expected exit 2, got {code}")
    require("error" in rep, "scenario 3 should report an 'error' field")

    # --- Scenario 4: click drives the component mouse-up path -> componentLuaMouseUp fires ---
    code, rep, raw = run(probe, "fixtures/fixture_probe_click.panel", "probe_click.json")
    require(rep is not None, f"scenario 4 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 4 expected exit 0, got {code}")
    require("clicked" in rep["steps"][0]["console"],
            f"scenario 4 click should log 'clicked' via componentLuaMouseUp: {rep['steps'][0]['console']}")

    # --- Scenario 5: sub-type value flips dependent controls' reported visibility ---
    code, rep, raw = run(probe, "fixtures/fixture_probe_visibility.panel", "probe_visibility.json")
    require(rep is not None, f"scenario 5 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 5 expected exit 0, got {code}")
    # value 100 (>= 64): change-count hidden, last-value shown.
    require(find_mod(rep, "change-count")["visible"] is False,
            "scenario 5 'change-count' should be hidden after setValue 100")
    require(find_mod(rep, "last-value")["visible"] is True,
            "scenario 5 'last-value' should be visible after setValue 100")
    require(len(rep["finalState"]["layers"]) >= 1, "scenario 5 report should carry layers")
    require("uid" in rep["finalState"]["layers"][0], "scenario 5 layers should carry a uid")

    # --- Scenario 6: console() capture + callLuaMethod with args + return value ---
    code, rep, raw = run(probe, "fixtures/fixture_probe_console.panel", "probe_console.json")
    require(rep is not None, f"scenario 6 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 6 expected exit 0, got {code}")
    require("hello 42" in rep["steps"][0]["console"],
            f"scenario 6 step 0 should capture console('hello 42'): {rep['steps'][0]['console']}")
    require(rep["steps"][1].get("returnValue") == "7:x:true",
            f"scenario 6 echo(7,'x',true) return should be '7:x:true': {rep['steps'][1].get('returnValue')}")

    # --- Scenario 7: setLayerVisible flips a layer's reported visibility ---
    code, rep, raw = run(probe, "fixtures/fixture_probe_layers.panel", "probe_layers.json")
    require(rep is not None, f"scenario 7 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 7 expected exit 0, got {code}")
    page2 = [l for l in rep["finalState"]["layers"] if l["name"] == "Page 2"]
    require(len(page2) == 1, "scenario 7 should have a 'Page 2' layer")
    require(page2[0]["visible"] is False, "scenario 7 'Page 2' should be hidden after setLayerVisible")

    # --- Scenario 8: a nullary callLuaMethod reports its return value ---
    # Used to go through CtrlrLuaMethodManager::call(), which discarded the return -- so callers
    # passed a dummy argument purely to reach the raw-Lua path and read a value back.
    code, rep, raw = run_script(probe, "fixtures/fixture_probe_console.panel", {"steps": [
        {"action": "callLuaMethod", "method": "echo"},
    ]})
    require(rep is not None, f"scenario 8 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require("returnValue" in rep["steps"][0],
            "scenario 8 a nullary callLuaMethod should still report returnValue")
    require(rep["steps"][0]["returnValue"] == "nil:nil:nil",
            f"scenario 8 echo() should return 'nil:nil:nil': {rep['steps'][0].get('returnValue')}")

    # --- Scenario 9: eval reaches panel internals; tables convert both ways ---
    code, rep, raw = run_script(probe, "fixtures/fixture_probe_console.panel", {"steps": [
        {"action": "eval", "lua": "return {1, 2, {a = 1, b = 'two'}}"},
        {"action": "eval", "lua": "string.rep('ab', 2)"},                  # REPL-style expression
        {"action": "eval", "lua": "local t = {} t.self = t return t"},     # cycle
        {"action": "callLuaMethod", "method": "echo", "args": [[10, 20], {"k": "v"}, 1]},
    ]})
    require(rep is not None, f"scenario 9 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 9 expected exit 0, got {code}: {[s['errors'] for s in rep['steps']]}")
    require(rep["steps"][0]["returnValue"] == [1.0, 2.0, {"a": 1.0, "b": "two"}],
            f"scenario 9 nested table should convert to JSON: {rep['steps'][0].get('returnValue')}")
    require(rep["steps"][1]["returnValue"] == "abab",
            f"scenario 9 a bare expression should yield its value: {rep['steps'][1].get('returnValue')}")
    require(rep["steps"][2]["returnValue"] == {"self": "<cycle>"},
            f"scenario 9 a self-referential table should report <cycle>: {rep['steps'][2].get('returnValue')}")
    # Non-scalar args used to silently arrive as nil; now they are real Lua tables.
    require(rep["steps"][3]["returnValue"].startswith("table:"),
            f"scenario 9 an array arg should reach Lua as a table: {rep['steps'][3].get('returnValue')}")

    # --- Scenario 10: a throwing method lands in errors[] as type 'lua' AND in errorCount ---
    # The old split (step.error vs step.luaErrors) let a test assert on luaErrorCount and pass
    # green while the method it called threw on every invocation.
    code, rep, raw = run_script(probe, "fixtures/fixture_probe_console.panel", {"steps": [
        {"action": "eval", "lua": "error('boom')"},
        {"action": "setValue", "modulator": "no-such-modulator", "value": 1},
    ]})
    require(rep is not None, f"scenario 10 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 1, f"scenario 10 expected exit 1, got {code}")
    require([e["type"] for e in rep["steps"][0]["errors"]] == ["lua"],
            f"scenario 10 a raising chunk should be a 'lua' error: {rep['steps'][0]['errors']}")
    require("boom" in rep["steps"][0]["errors"][0]["message"],
            f"scenario 10 the Lua message should survive: {rep['steps'][0]['errors']}")
    require([e["type"] for e in rep["steps"][1]["errors"]] == ["action"],
            f"scenario 10 a missing modulator should be an 'action' error: {rep['steps'][1]['errors']}")
    require(rep["steps"][0]["ok"] is False and rep["steps"][1]["ok"] is False,
            "scenario 10 both steps should be not-ok")
    s = rep["summary"]
    require(s["errorCount"] == 2 and s["luaRuntimeErrorCount"] == 1 and s["actionErrorCount"] == 1,
            f"scenario 10 errorCount must total every channel: {s}")

    # --- Scenario 11: a Lua setVisible(false) shows up in liveVisible/effectiveVisible ---
    # componentVisibility (what `visible` reports) is never written by comp:setVisible(), so the
    # tree-only report claimed visible: true for a control that was plainly hidden.
    code, rep, raw = run_script(probe, "fixtures/fixture_probe_setvisible.panel", {"steps": [
        {"action": "callLuaMethod", "method": "hideTarget"},
    ]})
    require(rep is not None, f"scenario 11 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 11 expected exit 0, got {code}: {rep['steps'][0]['errors']}")
    hidden = find_mod(rep, "last-value")
    require(hidden["visible"] is True,
            "scenario 11 the tree property should still say visible (that is the point)")
    require(hidden["liveVisible"] is False,
            f"scenario 11 liveVisible should be False after setVisible(false): {hidden}")
    require(hidden["effectiveVisible"] is False,
            f"scenario 11 effectiveVisible should be False after setVisible(false): {hidden}")
    shown = find_mod(rep, "change-count")
    require(shown["liveVisible"] is True and shown["effectiveVisible"] is True,
            f"scenario 11 an untouched control should stay visible: {shown}")
    require("canvasRectangle" in shown,
            f"scenario 11 a placed control should report absolute bounds: {shown}")

    # --- Scenario 12: two overlapping controls are reported in finalState.overlaps ---
    code, rep, raw = run(probe, "fixtures/fixture_probe_overlap.panel")
    require(rep is not None, f"scenario 12 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 12 expected exit 0, got {code}")
    laps = rep["finalState"]["overlaps"]
    require(len(laps) == 1, f"scenario 12 expected exactly one overlap: {laps}")
    require({laps[0]["a"], laps[0]["b"]} == {"change-count", "last-value"},
            f"scenario 12 wrong pair reported: {laps}")
    require(laps[0]["rect"] == "150 40 38 72", f"scenario 12 wrong intersection: {laps}")
    # The same panel's non-overlapping controls must not be reported.
    code, rep2, _ = run(probe, "fixtures/fixture_probe_setvisible.panel")
    assert rep2 is not None
    require(rep2["finalState"]["overlaps"] == [],
            f"scenario 12 a clean layout should report no overlaps: {rep2['finalState']['overlaps']}")

    # --- Scenario 13: screenshot writes a real PNG of the laid-out canvas ---
    shot = os.path.abspath("probe_selftest_shot.png")
    if os.path.exists(shot):
        os.unlink(shot)
    code, rep, raw = run_script(probe, "fixtures/fixture_probe_setvisible.panel", {"steps": [
        {"action": "screenshot", "file": shot},
    ]})
    require(rep is not None, f"scenario 13 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 13 expected exit 0, got {code}: {rep['steps'][0]['errors']}")
    got = rep["steps"][0].get("screenshot")
    require(got is not None, f"scenario 13 step should report a screenshot: {rep['steps'][0]}")
    require(got["width"] > 0 and got["height"] > 0, f"scenario 13 empty image: {got}")
    require(os.path.exists(shot) and os.path.getsize(shot) > 0,
            "scenario 13 should have written a non-empty PNG")
    with open(shot, "rb") as f:
        require(f.read(8) == b"\x89PNG\r\n\x1a\n", "scenario 13 output is not a PNG")
    os.unlink(shot)

    # --- Scenario 14: settle is a ceiling, not a floor (regression guard on the speed fix) ---
    # 30 no-op steps used to cost ~450 ms each (~13.5 s). They must now finish well inside 3 s of
    # step time. Console output must still be captured -- CtrlrLog is an AsyncUpdater, so a settle
    # that skipped pumping entirely would silently drop it.
    started = time.monotonic()
    code, rep, raw = run_script(probe, "fixtures/fixture_probe_console.panel", {
        "steps": [{"action": "callLuaMethod", "method": "sayHello"} for _ in range(30)]})
    elapsed = time.monotonic() - started
    require(rep is not None, f"scenario 14 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(code == 0, f"scenario 14 expected exit 0, got {code}")
    require(len(rep["steps"]) == 30, "scenario 14 expected 30 steps")
    require(elapsed < 8.0, f"scenario 14 30 no-op steps took {elapsed:.1f}s -- settle has regressed")
    require(all("hello 42" in s["console"] for s in rep["steps"]),
            "scenario 14 every step must still capture its console() output")

    # --- Scenario 15: waitFor polls until truthy, and reports a timeout as an action error ---
    code, rep, raw = run_script(probe, "fixtures/fixture_probe_console.panel", {"steps": [
        {"action": "eval", "lua": "probe_t0 = os.time()"},
        {"action": "waitFor", "lua": "return os.time() > probe_t0", "timeoutMs": 5000, "pollMs": 20},
        {"action": "waitFor", "lua": "return false", "timeoutMs": 100, "pollMs": 20},
    ]})
    require(rep is not None, f"scenario 15 stdout was not valid JSON:\n{raw}")
    assert rep is not None
    require(rep["steps"][1]["returnValue"] is True and rep["steps"][1]["errors"] == [],
            f"scenario 15 waitFor should succeed once the condition goes true: {rep['steps'][1]}")
    require([e["type"] for e in rep["steps"][2]["errors"]] == ["action"],
            f"scenario 15 a waitFor timeout should be an action error: {rep['steps'][2]['errors']}")

    # --- Scenario 16: --stdin runs several scripts in one process, one report per line ---
    code, reports = run_stdin(probe, "fixtures/fixture_probe_console.panel", [
        {"steps": [{"action": "eval", "lua": "probe_marker = 'first'"}]},
        {"steps": [{"action": "eval", "lua": "return probe_marker"}]},
        {"steps": [{"action": "callLuaMethod", "method": "sayHello"}]},
    ])
    require(code == 0, f"scenario 16 --stdin should exit 0 at EOF, got {code}")
    require(len(reports) == 3, f"scenario 16 expected one report per script, got {len(reports)}")
    require(all(r["reportVersion"] == 1 for r in reports), "scenario 16 every report needs a version")
    # Documented contract: scripts in one process SHARE state. Isolation means respawning.
    require(reports[1]["steps"][0]["returnValue"] == "first",
            f"scenario 16 state should persist across scripts: {reports[1]['steps'][0]}")
    require("hello 42" in reports[2]["steps"][0]["console"],
            f"scenario 16 later scripts still capture console: {reports[2]['steps'][0]}")

    print("SELFTEST PASS: all 16 scenarios ok")
    sys.exit(0)


if __name__ == "__main__":
    main()
