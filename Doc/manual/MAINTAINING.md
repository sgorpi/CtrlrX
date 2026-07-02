# Maintaining the CtrlrX Manual — guide for agents (and humans)

This is the working guide for **updating** the manual under `Doc/manual/`. Read this **before** editing any chapter.

---

## 1. What the manual is (don't drift from this)

A maintainable, version-controlled Markdown manual that takes a beginner from "I installed CtrlrX" to
"I have a working panel that talks to my synth," plus a curated Lua guide + reference. One continuous
example panel ("ExampleSynth", saved at [example/first-panel.panel](example/first-panel.panel)) is
grown across chapters 3–8. No hardware required to follow along (virtual MIDI port + MIDI Monitor).

File tree:

```
README.md                 index / how-to-read
01..10-*.md               tutorial chapters (see README table for the map)
lua/01-lua-guide.md       Lua language primer for programmers
lua/02-lua-reference.md   curated CtrlrX + JUCE Lua API + callback hooks
images/                   PNG screenshots
example/first-panel.panel the finished tutorial panel
MANUAL-PROGRESS.md        status + screenshot manifest + feedback-round log
MAINTAINING.md            this file
```

---

## 2. The two golden rules

1. **Verify every fact against the source — never copy from memory, from the old `.docx`, or from
   forum/PR comments.** Those use stale names and spellings. The source-of-truth map (§4) tells you
   which file backs which topic. Cite the source file (with a repo-relative link) in a `> 🔗 Deeper`
   callout so the next maintainer can re-check.
2. **Match the house style (§3) exactly.** The manual's value is that one document serves both
   thorough learners and quick-answer seekers. Don't introduce a different structure per chapter.

---

## 3. House style (locked decisions)

- **Layered / progressive disclosure.** Every chapter opens with a **TL;DR** box (2–4 bullets: what
  it is / when you need it).
- **Tasks are goal-titled recipes:** one-line goal → numbered **Steps** (the quick answer) → a **How
  it works** subsection (the depth) → **See also** / cross-links.
- **Reference material is scannable tables, not prose.** (Constants, functions, message types,
  component types, encodings — all tables.)
- **Consistent callouts:** `> 💡 Tip`, `> ⚠️ Gotcha`, `> 🔗 Deeper`.
- **Heavy cross-linking** between tutorial chapters and the Lua reference.
- **Menu paths** as **Menu → Submenu → Item**. Code blocks are Lua unless noted.
- **File/code references** use markdown links, relative to `Doc/manual/`
  (e.g. `[Chapter 8](08-sending-receiving.md)`, `[CtrlrMacros.h](../../Source/Core/CtrlrMacros.h)`).
- Match the surrounding chapters' tone and density; when in doubt, open a sibling chapter and mirror it.

---

## 4. Source-of-truth map (verify here, don't guess)

| Topic | Source of truth |
|---|---|
| **Component types** (uiSlider, uiCombo, …) & the "Add component" grouping | `Source/UIComponents/CtrlrComponents/CtrlrComponentTypeManager.cpp` (`getComponentMenu()`) |
| **Modulator MIDI message types** (CC, Multi, SysEx, clock…) | `Source/MIDI/CtrlrMidiMessage.h` |
| **Expression constants** (`modulatorValue`, `modulatorMappedMax`, `midiValue`, `vstIndex`…) | `CtrlrModulatorProcessor::getSymbolValue` in `Source/Core/CtrlrModulator/CtrlrModulatorProcessor.cpp` |
| **Expression functions** (`gte/gt/lt/lte/eq`, `mod/fmod`, bit helpers, `setGlobal`, `getModulatorBy*Index`) | `evaluateFormulaFunction` in `Source/Core/CtrlrPanel/CtrlrEvaluationScopes.cpp` |
| **Expression property labels & defaults** (`modulatorValueExpression` etc.) | labels/types in `Source/Resources/XML/CtrlrIDs.xml`; defaults `EXP_MODULATOR_*` in `Source/Core/CtrlrModulator/CtrlrModulator.h` |
| **The expression editor UI** (formula box + 🐞 validate button) | `CtrlrExpressionProperty` in `Source/UIComponents/CtrlrPropertyEditors/CtrlrPropertyComponent.cpp` |
| **SysEx tokens / checksums** | `CtrlrSysexToken` / `CtrlrSysexProcessor` in `Source/Core/` |
| **Byte encodings for bulk data** (`CtrlrByteEncoding`) | enum in `Source/Core/CtrlrMacros.h`; Lua names (`CtrlrPanel.Encode*`) in the `enum_("CtrlrByteEncoding")` block of `Source/Lua/CtrlrLuaManager.cpp` |
| **Bulk data methods** (`getModulatorValuesAsData`, `setModulatorValuesFromData`) | definitions + luabind `.def(...)` registration in `Source/Lua/CtrlrLuaManager.cpp` |
| **Lua globals** (`panel`, `utils`, `timer`, `devices`, …) | `assignDefaultObjects` in `Source/Lua/CtrlrLuaManager.cpp` |
| **Lua class methods** (CtrlrPanel/Modulator/Component/MidiMessage/…) | luabind registrations in `Source/Lua/CtrlrLuaManager.cpp` and `Source/UIComponents/CtrlrComponents/CtrlrComponentLuaRegistration.cpp` |
| **Callback hooks** (panel/modulator/midi/mouse/paint/timer, `source` enum) | `Source/Resources/XML/CtrlrLuaMethodTemplates.xml` |
| **MIDI Thru / routing** (5 routes + channelize) | `Source/UIComponents/CtrlrMIDI/CtrlrMIDISettingsRouting.cpp` (MIDI Settings dialog → **Routing** tab) |
| **Property IDs / names in general** | `Source/Core/CtrlrIDs.h` + `Source/Resources/XML/CtrlrIDs.xml` |

> Use clangd LSP (`workspaceSymbol` / `goToDefinition` / `findReferences`) to navigate C++ rather than
> grep. Grep is fine for the XML resource files and label strings.

When a Lua method has **multiple overloads** (e.g. `getModulatorValuesAsData`), read all of them and
document the signatures you actually recommend; note the overload set in the reference table.

---

## 5. Standard update workflow

Use this for any change (new topic, correction, or a feedback round):

1. **Read** this file, then the chapter(s) you'll touch. Note existing
   headings so you expand rather than duplicate.
2. **Locate the target chapter(s)** by topic — the README "How to read this" table is the map. New
   material almost always belongs *inside* an existing chapter; add a whole chapter only if the plan's
   scope clusters genuinely don't cover it.
3. **Verify against source** (§4). Confirm names, signatures, enum spellings, UI labels, defaults.
   Prefer citing exact behavior (e.g. "invalid → box turns pink + parse-error alert") over vague
   description.
4. **Draft in house style** (§3): TL;DR if it's a new chapter/major section, recipe + tables, callouts,
   cross-links to sibling chapters and the Lua reference. Add reciprocal links (if ch08 points to the
   Lua reference anchor, make sure that anchor exists).
5. **Screenshots** (§6) — reference them even before capture with `![alt](images/NAME.png)` and add a
   row to the manifest; capture in a later pass.
6. **Verify** (§7) before claiming done.

---

## 6. Screenshots

State-dependent shots need the standalone app driven to the right state, so these are **captured
interactively with the user** (or by the user loading a prepared panel and saying when ready). The
app runs on `DISPLAY=:1`.

Working method (reliable):

```bash
DISPLAY=:1 wmctrl -l | grep -i ctrlr           # get the window id (WID)
DISPLAY=:1 import -window <WID> Doc/manual/images/<name>.png
```

`import -window <WID>` grabs the CtrlrX client area cleanly regardless of monitor/position. `scrot`
and `gnome-screenshot` are also available.

Fragile: scripted `xdotool` menu/dialog clicks — the window may be on a second monitor with WM
decoration (frame-vs-client offset breaks absolute coords) and JUCE popups close on focus change. So:
**have the user set up menus/dialogs/property selections by hand, then capture.**

After capturing: embed the image (replace any `<!-- TODO capture -->` placeholder with the
`![alt](images/name.png)` tag) and mark done. Don't plan screenshots for pure
Lua/console workflows that have no distinct UI.

---

## 7. Verification checklist (before saying "done")

- **Internal links & image paths resolve** (relative to the file's directory — links inside `lua/*`
  are relative to `lua/`).
- **Anchors match GitHub slug rules**: lowercase, punctuation like `&` dropped, spaces → hyphens; a
  spaced em-dash in a heading becomes a *double* hyphen (e.g. `## Bulk dumps — many …` →
  `#bulk-dumps--many-`). Test cross-file `file.md#anchor` links against the actual heading.
- **Source accuracy spot-check**: re-open the source file for at least the trickiest new claim.
- **Reciprocal Lua-reference anchors exist** for any `lua/02-lua-reference.md#...` link you added.
- **Table of contents (TOC) is up-to-date**: All sections of a chapter are listed in the TOC and 
  the TOC does not contain stale links to sections.
- Don't claim completeness you haven't checked; if a screenshot is deferred, say so.

---

## 8. Handling a feedback round (e.g. a GitHub PR thread)

1. Read the comments (`gh pr view <N> --repo <owner>/<repo> --json comments --jq ...`). Separate
   *praise* from *actionable gaps*.
2. Cluster the asks and map each to a target chapter + a source-of-truth entry (§4).
3. Reconcile terminology: forum/PR text uses **older names** — always cross-check spellings against
   source before writing (this bit us with the `Encode*` names).
4. Draft (§5), verify (§7), and only then consider replying to the thread — **never post to an external
   service or push without explicit user go-ahead.**
5. Reference material the maintainers point at (e.g. the CtrlrX wiki) can be *linked* rather than
   re-hosted. Known wiki references used by the manual: SysEx Token List, Ctrlr MIDI Thru
   capabilities, Source filter with LUA scripts, and the macOS/troubleshooting pages (see the
   Troubleshooting section of [10-distribution.md](10-distribution.md)).

---

## 9. Repo constraints worth knowing

- The manual is **prose + PNGs only**; it doesn't build or ship with the plugin, so C++ standard /
  warning constraints don't apply to editing it.
- Keep everything **GitHub-renderable** (the manual is read straight from the repo tree, e.g.
  `.../tree/documentation/Doc/manual#readme`). Avoid HTML where Markdown suffices.
- Commit only when asked; don't push or touch the PR without explicit approval.
- The app is cross-platform, if examples rely on specific OS features then give examples for all platforms.
