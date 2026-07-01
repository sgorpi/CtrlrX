# CtrlrX User Manual — Create Your First Panel

> **TL;DR**
> - CtrlrX lets you build a custom on-screen control surface (a **panel**) for any MIDI hardware or software.
> - This manual takes you from zero to a working panel that talks to your gear, then teaches you to script it with **Lua**.
> - Read it cover-to-cover to learn thoroughly, or jump to a chapter and read the **TL;DR + Steps** for a quick answer.

This manual is written in layers. Every chapter starts with a **TL;DR** box. Tasks are written as
recipes: a one-line goal, numbered **Steps** (the quick answer), then a **How it works** section
(the depth). Skim the steps, or read everything — your choice.

> 💡 **Get CtrlrX:** download a prebuilt **Windows / macOS / Linux** binary from the
> [releases page](https://github.com/damiensellier/CtrlrX/releases). Install steps are in
> [Chapter 1](01-getting-started.md).

## How to read this

| You want to… | Start here |
|---|---|
| Understand what CtrlrX is and install it | [01 — Getting Started](01-getting-started.md) |
| Learn the vocabulary before clicking around | [02 — Core Concepts](02-core-concepts.md) |
| Build a panel right now | [03 — Your First Panel](03-first-panel.md) |
| Know which on-screen control to use | [04 — GUI Elements](04-gui-elements.md) |
| Map a control's range to real values | [05 — Value Mapping](05-value-mapping.md) |
| Connect MIDI devices | [06 — MIDI Basics](06-midi-basics.md) |
| Make a control send MIDI or run Lua | [07 — Making Elements Responsive](07-making-responsive.md) |
| Send/receive CC, NRPN, SysEx | [08 — Sending & Receiving MIDI](08-sending-receiving.md) |
| Figure out why something isn't working | [09 — Debugging](09-debugging.md) |
| Save, share, or run your panel as a plugin | [10 — Distribution & Advanced](10-distribution.md) |
| Fix install / macOS / crash-on-startup issues | [10 — Troubleshooting & platform notes](10-distribution.md#troubleshooting--platform-notes) |
| Learn Lua scripting | [Lua Guide](lua/01-lua-guide.md) · [Lua API Reference](lua/02-lua-reference.md) |

## The example panel

Chapters 3–8 build **one** small panel step by step for a fictional synth ("ExampleSynth"). You
don't need any hardware to follow along — we use a virtual MIDI port and the built-in MIDI Monitor.
The finished result is saved at [example/first-panel.panel](example/first-panel.panel).

## Conventions used in this manual

- `> 💡 Tip` — a shortcut or good habit.
- `> ⚠️ Gotcha` — a common mistake or surprising behavior.
- `> 🔗 Deeper` — a pointer to a fuller explanation elsewhere.
- Menu paths are written as **Menu → Submenu → Item**.
- Code blocks are Lua unless noted otherwise.

---

*This is community documentation for CtrlrX, a maintained fork of Roman Kubiak's Ctrlr. Corrections
and additions via pull request are welcome.*
