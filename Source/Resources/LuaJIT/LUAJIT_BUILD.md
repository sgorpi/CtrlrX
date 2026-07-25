# Building LuaJIT for CtrlrX (Windows x64)

## Overview

LuaJIT is included as source in `Source/Misc/luajit` with a pre-built `lua51.lib` already committed to the repository. Under normal circumstances **you do not need to rebuild it**.

You only need to rebuild if:
- You are upgrading LuaJIT to a newer version
- The committed `lua51.lib` was accidentally built for x86 (see warning below)
- You are switching between Debug/Release configurations of LuaJIT itself

---

## ⚠️ Critical Warning: You MUST use the x64 Native Tools Command Prompt

**Do NOT run `msvcbuild.bat` from:**
- Regular CMD (`cmd.exe`)
- PowerShell
- Windows Terminal (unless configured for x64)

These shells have no MSVC environment set up, and `msvcbuild.bat` will silently produce an **x86 library** without any warning. This will cause dozens of `LNK2001 unresolved external symbol` linker errors when building CtrlrX, even though `lua51.lib` appears to exist and be valid.

**You MUST run `msvcbuild.bat` from:**

> **Start Menu → Visual Studio 2022 → x64 Native Tools Command Prompt for VS 2022**

This shell sets up the correct 64-bit MSVC environment (`cl.exe`, `link.exe`, `lib.exe`) before you run the build.

---

## Build Steps

1. Open **x64 Native Tools Command Prompt for VS 2022** from the Start Menu

2. Navigate to the LuaJIT source directory:
    ```cmd
    cd C:\path\to\CtrlrX\Source\Misc\luajit\src
    ```

3. Build the static library:
    ```cmd
    msvcbuild.bat static
    ```

4. Verify the output is x64 (not x86):
    ```cmd
    dumpbin /headers lua51.lib | findstr machine
    ```
    You should see:
    ```
    8664 machine (x64)
    ```
    If you see `14C machine (x86)` you used the wrong shell — start over from step 1.

5. Commit the new `lua51.lib` to the repository:
    ```cmd
    git add Source/Misc/luajit/src/lua51.lib
    git commit -m "Rebuild LuaJIT lua51.lib as x64"
    ```

---

## After Rebuilding

Go to Visual Studio and run **Project → Delete Cache and Reconfigure**, then rebuild CtrlrX. This ensures CMake picks up the new `lua51.lib`.

---

## Verifying LuaJIT is Active at Runtime

In the CtrlrX Lua console:
```lua
console(String(jit.version()))  -- should print e.g. "LuaJIT 2.1.0-beta3"
```

# Linux

````cd Source/Misc/luajit/src````<br>
```make clean```<br>
```make -j$(nproc) XCFLAGS="-fPIC"``` <br>
```cd ../../../../```<br>
```./autobuild_ninja```<br>

# MacOS

```
cd Source/Misc/luajit/src
MACOS_DEPLOYMENT_TARGET=14.0 make

```
----

The CtrlrX About window also displays the Lua/LuaJIT version if LuaJIT is enabled.
