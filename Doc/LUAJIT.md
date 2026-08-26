# LuaJIT in CtrlrX

CtrlrX embeds LuaJIT. The full upstream source is vendored at `Source/Misc/luajit/` and **built
from source by CMake** on Linux, macOS and Windows. There are no prebuilt LuaJIT binaries in this
repository, and there should never be any.

> This file lives in `Doc/`, not `Source/Resources/`. Everything under `Source/Resources/` is
> globbed into the binary as JUCE `BinaryData` by `cmake/Assets.cmake` — documentation put there
> ships inside every plugin.

## Layout

| Path | What it is |
|---|---|
| `Source/Misc/luajit/` | unmodified upstream LuaJIT 2.1 ROLLING (a553b3de) |
| `cmake/luajit/CMakeLists.txt` | **fork-local.** Upstream ships no CMake build at all |
| `cmake/luajit/check_makefile_drift.py` | CI check: the source lists below still match `src/Makefile` |
| `Doc/LUAJIT.md` | this file |

The build lives outside the tree it builds, deliberately: `Source/Misc/luajit/` can be replaced
wholesale on an upgrade without deleting the build. `CMakeLists.txt` finds the sources through
`CTRLRX_LUAJIT_SOURCE_DIR`, which defaults to `Source/Misc/luajit`.

Everything generated during the build goes to `<builddir>/cmake/luajit/gen/`, never into the source
tree.

## How the build works

LuaJIT cannot be built by pointing CMake at `*.c`. It has a two-stage bootstrap, and
`cmake/luajit/CMakeLists.txt` reproduces it:

```
minilua                                       host tool, arch-agnostic     built once
minilua genversion.lua                      → luajit.h                     once
minilua dynasm.lua <DASM_FLAGS> vm_<a>.dasc → host/buildvm_arch.h          per arch
buildvm = host/buildvm*.c + buildvm_arch.h    host exe, target-aware       per arch
buildvm -m elfasm|machasm|peobj             → lj_vm.S / lj_vm.s / lj_vm.obj per arch
buildvm -m bcdef|ffdef|libdef|recdef        → lj_*def.h                    per arch
buildvm -m folddef                          → lj_folddef.h                 per arch
compile LJCORE_C + lj_vm                    → libluajit_<slice>            per arch
lipo (Apple, >1 arch)                       → libluajit
```

Only `minilua` is architecture-agnostic. `buildvm` is a **host** executable that embeds the
**target** architecture's VM description, which is why a macOS universal build runs the chain once
per slice and merges the results with `lipo`.

The `-m` mode for the VM object differs by platform: `elfasm` on Linux, `machasm` on Apple, and
`peobj` on Windows, because MSVC has no assembler for the generated code and consumes a ready-made
PE object instead.

### Where the flags come from

On Clang/GCC, architecture and DynASM flags are derived from a preprocessor probe, exactly as
upstream's Makefile does it:

```sh
cc -E -dM Source/Misc/luajit/src/lj_arch.h
```

The dump is matched for `LJ_TARGET_X64`, `LJ_ARCH_BITS`, `LJ_HASJIT`, `LJ_HASFFI`, `LJ_DUALNUM`,
`LJ_ARCH_HASFPU`, `LJ_ABI_SOFTFP`, `LJ_NO_UNWIND`, `LJ_ABI_PAUTH`, `LJ_ABI_BRANCH_TRACK`,
`LJ_ABI_SHADOW_STACK`, `LJ_ARCH_VERSION` and `LJ_FR2`. This is the part that survives upgrades: if
upstream flips a default, the probe picks it up with no edit here.

**MSVC cannot dump macros**, so Windows uses a hardcoded table transcribed from
`src/msvcbuild.bat`. Upstream does the same thing. That table is the one place that must be
re-checked by hand on an upgrade — everything transcribed from `src/Makefile` is checked by
`check_makefile_drift.py` instead.

## Options

| Option | Default | Effect |
|---|---|---|
| `CTRLRX_USE_LUAJIT` | `ON` | `OFF` is not supported and fails at configure time |
| `CTRLRX_LUAJIT_USE_SYSTEM` | `OFF` | Use a system LuaJIT via pkg-config instead of the vendored source |
| `CTRLRX_LUAJIT_DEBUG` | `OFF` | Build LuaJIT unoptimised, for debugging LuaJIT itself |
| `CTRLRX_LUAJIT_SOURCE_DIR` | `Source/Misc/luajit` | Which LuaJIT source tree to build |

LuaJIT is otherwise always built `-O2` regardless of `CMAKE_BUILD_TYPE`, matching upstream's
`CCOPT`, so a Debug CtrlrX still runs panel Lua at full speed. Debug info still follows the build
type.

`CTRLRX_LUAJIT_USE_SYSTEM=ON` exists for distro packagers and for A/B-ing against the vendored
build when chasing a Lua-level bug. Be aware it means that platform runs a *different* LuaJIT from
the others — Ubuntu 24.04 ships 2.1.0+git20231223, roughly two and a half years older than what is
vendored here.

## Building LuaJIT on its own

`cmake/luajit/CMakeLists.txt` configures standalone as well as from the CtrlrX build, so LuaJIT can
be produced without configuring the rest of CtrlrX (JUCE, boost, the plugin formats):

```sh
cmake -S cmake/luajit -B /tmp/lj
cmake --build /tmp/lj -j"$(nproc)"
```

That leaves the static library in `/tmp/lj/` (`libluajit_native.a` on a single-architecture
platform) and the generated headers in `/tmp/lj/gen/`. Add `-DCTRLRX_LUAJIT_SOURCE_DIR=<path>` to
build a different checkout, and the usual `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"` for a universal
macOS build.

This is the hook for a `.jucer` pre-build step: the Projucer/Xcode exporters do not run CMake, so a
pre-build phase that invokes the two commands above gives them the same `libluajit` the CMake build
produces. Note the stale-file guard below — it rejects a source tree that still contains output from
LuaJIT's own `make`.

## Upgrading LuaJIT

1. Replace the contents of `Source/Misc/luajit/` with the new upstream tree. The whole directory
   is upstream, so this is a straight wipe-and-drop-in — the fork-local build lives in
   `cmake/luajit/` and is not touched.
2. Do not commit any build output. If you ran `make` or `msvcbuild.bat` in the tree while testing,
   clean it first:
   ```sh
   git clean -xdf Source/Misc/luajit
   ```
   The build has a configure-time guard for this, so a stale tree fails loudly rather than silently
   miscompiling — see below.
3. Reconcile `cmake/luajit/CMakeLists.txt` with the new `src/Makefile`. CI checks this, and it
   runs locally too:
   ```sh
   python3 cmake/luajit/check_makefile_drift.py
   ```
   It compares `LJ_LIB_C`, `LJ_CORE_C` and `LJ_BUILDVM_C` against the Makefile's `LJLIB_O`,
   `LJCORE_O` and `BUILDVM_O` **as ordered lists** — `LJLIB_C`'s order fixes fast-function and
   bytecode numbering, so a reordering is a silent miscompile, not a build error — and the
   `DASM_AFLAGS` tokens against those `_luajit_detect()` emits. Fix `CMakeLists.txt` until it exits
   0; there is no second file to bless.

   Tokens for architectures CtrlrX does not build (ppc, mips, iOS-on-arm) are allowlisted in
   `UNSUPPORTED_ARCH_TOKENS` in that script. If upstream adds a token for an architecture we *do*
   build, handle it in `_luajit_detect()`.
4. Re-check by hand, which the script does not cover:
   - the MSVC table in `_luajit_detect()`, against the new `src/msvcbuild.bat`
   - `LJVM_MODE` per target system
5. Build and test on all three platforms.

### Version string

`CtrlrAbout.cpp` displays `LUAJIT_VERSION` from the generated `luajit.h`, which currently reads
`LuaJIT 2.1.ROLLING`.

`genversion.lua` substitutes `ROLLING` with digits read from `luajit_relver.txt`, which upstream
fills from `git show -s --format=%ct` **in the LuaJIT checkout**. `Source/Misc/luajit/` is a plain
directory here, not a submodule, so running that would report *CtrlrX's* commit time and put a
number in the About box that looks like a LuaJIT version but is not one. The build therefore feeds
`genversion.lua` the shipped `.relver`, which contains the unexpanded `$Format:%ct$` git-archive
placeholder; no digits are found and `ROLLING` is kept.

If you ever want a real version there, set the vendored revision's true upstream commit timestamp
in `luajit_relver.txt` at build time — and add bumping it to this checklist.

## Troubleshooting

### "stale generated files found in …"

You ran LuaJIT's own `Makefile` or `msvcbuild.bat` by hand and left generated files in `src/`.
Because a quoted `#include "lj_bcdef.h"` searches the including file's own directory *before* any
`-I`, those files shadow the ones CMake generates and produce a miscompile rather than an error.
The configure-time guard catches this. Fix:

```sh
git clean -xdf Source/Misc/luajit
```

### Running the arch probe by hand

When the probe misfires, reproduce what CMake does:

```sh
cd Source/Misc/luajit/src
cc -I . -E -dM lj_arch.h | grep -E 'LJ_TARGET_|LJ_ARCH_|LJ_ABI_|LJ_HAS|LJ_FR2|LJ_LE|LJ_NO_UNWIND'
```

For a macOS cross-slice, add the arch flag the slice uses, e.g. `cc -arch x86_64 ...`.

### Unsupported architecture

The build supports x86, x64, arm and arm64. Anything else fails at configure time with a pointer to
`CTRLRX_LUAJIT_USE_SYSTEM=ON`. Upstream supports more (ppc, mips, s390x) and the vendored `.dasc`
files are present, so extending `_luajit_detect()` is mostly a matter of adding the arch to the
match chain.

### `-LN` on Windows

`msvcbuild.bat` passes `-LN` to dynasm; the Makefile does not (`DASM_XFLAGS` is empty). This build
follows the Makefile on every platform. If Windows-specific codegen problems ever appear, that
discrepancy is worth revisiting.
