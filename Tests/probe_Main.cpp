// CtrlrPanelProbe -- drives a panel from a script and reports what it did. Headless one-shot:
// no hardware, no display.
//
// Loads an arbitrary .panel, rebinds its MIDI devices to the mocked test devices, (optionally)
// runs a JSON action script, and prints a JSON report of everything observed. See
// Doc/PanelProbe.md for the invocation / script / report contract.
//
// This reuses the GoogleTest mock MIDI layer (mock_MidiDevice*.cpp) but is NOT a gtest binary:
// it drives a real CtrlrProcessor the same way test_ProcessorFixture.h does, outside any TEST.
//
// Exit codes: 0 = clean; 1 = a Lua compile/runtime error or step error occurred (report still
// complete); 2 = panel load or script parse failure (minimal JSON error on stdout).

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <iostream>
#include <mutex>
#include <set>
#include <vector>
#include <fcntl.h>

// The report is written to a saved copy of fd 1 (see redirectStdoutToStderr). MSVC spells the
// POSIX fd calls with a leading underscore in <io.h>, and has no /dev/null or ssize_t.
#ifdef _WIN32
  #include <io.h>
#else
  #include <unistd.h>
#endif

namespace probe_fd
{
#ifdef _WIN32
    constexpr int stdoutFd = 1, stderrFd = 2;
    constexpr const char* nullDevice = "NUL";
    inline int dup (int fd)                                { return ::_dup (fd); }
    inline int dup2 (int from, int to)                     { return ::_dup2 (from, to); }
    inline int openWrite (const char* path)                { return ::_open (path, _O_WRONLY); }
    inline int close (int fd)                              { return ::_close (fd); }
    inline long write (int fd, const char* buf, size_t n)  { return ::_write (fd, buf, (unsigned int) n); }
    // Binary mode, so the report's "\n" does not become "\r\n" (one report per line in --stdin).
    inline void setBinary (int fd)                         { ::_setmode (fd, _O_BINARY); }
#else
    constexpr int stdoutFd = STDOUT_FILENO, stderrFd = STDERR_FILENO;
    constexpr const char* nullDevice = "/dev/null";
    inline int dup (int fd)                                { return ::dup (fd); }
    inline int dup2 (int from, int to)                     { return ::dup2 (from, to); }
    inline int openWrite (const char* path)                { return ::open (path, O_WRONLY); }
    inline int close (int fd)                              { return ::close (fd); }
    inline long write (int fd, const char* buf, size_t n)  { return (long) ::write (fd, buf, n); }
    inline void setBinary (int)                            {}
#endif
}

#include "gmock/gmock.h"
#include "JuceHeader.h"

#include <CtrlrProcessor.h>
#include <CtrlrManager.h>
#include <CtrlrPanel.h>
#include <CtrlrModulator.h>
#include <CtrlrModulatorProcessor.h>
#include "CtrlrComponents/CtrlrComponent.h"
#include "CtrlrPanel/CtrlrPanelCanvas.h"
#include <CtrlrMIDIDeviceManager.h>
#include "CtrlrLuaManager.h"
#include "Methods/CtrlrLuaMethodManager.h"
#include "Methods/CtrlrLuaMethod.h"
#include "CtrlrIDs.h"
#include "CtrlrLog.h"
#include "CtrlrMacros.h"

// Raw Lua 5.1 C API for callLuaMethod-with-args. Lua is vendored with C linkage
// (LUABIND_CPLUSPLUS_LUA is not defined), so wrap in extern "C". This is deliberately the
// stable C API (not luabind): it is exactly what a future LuaJIT switch honours, and it
// avoids dragging the luabind template layer into this C++17 translation unit.
extern "C"
{
    #include "lua.h"
    #include "lauxlib.h" // luaL_loadstring, for the `eval` / `waitFor` actions
}

#include "mock_MidiDevice.h"
#include "probe_Report.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;

namespace
{
    constexpr int BLOCK_SIZE = 1024; // matches test_ProcessorFixture.h's audio block size

    // Stamped on every report (and on the fatal-error object) so a consumer can tell which
    // schema it is looking at. Bump on any breaking change to the report shape.
    constexpr int PROBE_REPORT_VERSION = 1;

    // Settle defaults. settleMs is a *ceiling*, not a floor: a step returns as soon as the panel
    // has been quiet (no device send, no host MIDI, no log line) for quietMs. See settle().
    constexpr int DEFAULT_SETTLE_MS = 300;
    constexpr int DEFAULT_QUIET_MS  = 30;
    constexpr int SETTLE_SLICE_MS   = 5; // granularity of one pump + processBlock iteration

    // CtrlrX pops a modal AlertWindow ("Callback error ...") on a runtime Lua error (see
    // CATCH_METHOD_EXCEPTION in CtrlrLuaMethodManagerCalls.cpp). That is a *blocking* modal loop on
    // the message thread -- fatal for a headless one-shot tool: it would hang waiting for a click
    // (or spin forever on a display-less CI). We can't touch that production code, so instead we run
    // a timer that dismisses any modal component the instant it appears. The timer fires from inside
    // the nested modal dispatch loop, so the blocked call returns immediately. The error is still
    // recorded (setValid(false)) and logged to stderr; only the dialog is suppressed.
    struct ModalDialogSuppressor : private juce::Timer
    {
        ModalDialogSuppressor() { startTimer(5); }
        ~ModalDialogSuppressor() override { stopTimer(); }
        void timerCallback() override
        {
            if (auto* m = juce::ModalComponentManager::getInstanceWithoutCreating())
                m->cancelAllModalComponents();
        }
    };

    // fd of the real stdout, saved before we redirect fd 1 to stderr. CtrlrX / JUCE / Lua write
    // plenty of noise to stdout during load; the report on stdout must be clean JSON, so all that
    // noise is sent to stderr and only the final report is written back to this saved fd.
    int g_realStdout = -1;

    // Redirect fd 1 (stdout) to fd 2 (stderr) so library chatter can't corrupt the JSON on stdout.
    void redirectStdoutToStderr()
    {
        std::cout.flush();
        std::fflush(stdout);
        g_realStdout = probe_fd::dup(probe_fd::stdoutFd);
        if (g_realStdout >= 0)
            probe_fd::setBinary(g_realStdout);
        probe_fd::dup2(probe_fd::stderrFd, probe_fd::stdoutFd);
    }

    // Quiet mode (default, no --verbose): send fd 1 and fd 2 to /dev/null so none of CtrlrX /
    // JUCE / Lua's stderr chatter (all of it lands on stderr via Logger::outputDebugString)
    // is shown. The report still goes to the saved g_realStdout fd, so stdout stays the clean
    // JSON contract and exit codes are unaffected.
    void silenceStdErr()
    {
        std::cout.flush();
        std::fflush(stdout);
        std::fflush(stderr);
        const int devnull = probe_fd::openWrite(probe_fd::nullDevice);
        if (devnull >= 0)
        {
            probe_fd::dup2(devnull, probe_fd::stdoutFd);
            probe_fd::dup2(devnull, probe_fd::stderrFd);
            probe_fd::close(devnull);
        }
    }

    // Captures CtrlrLog messages so console() output and real Lua error text land in the
    // report instead of only on stderr. messageLogged() is delivered on the message thread
    // during settle()'s dispatch-loop pumping, so it is read back single-threaded (no lock).
    struct LogCapture : public CtrlrLog::Listener
    {
        struct Entry { CtrlrLog::LogLevel level; juce::String message; };
        std::vector<Entry> entries;
        void messageLogged(CtrlrLog::CtrlrLogMessage m) override
        {
            entries.push_back({ m.level, m.message });
        }
    };

    // How deep pushVarToLua / luaToVar will descend before giving up. Guards against a
    // self-referential Lua table (belt and braces alongside the cycle set) and against a
    // pathologically nested JSON arg blowing the C stack.
    constexpr int MAX_CONVERSION_DEPTH = 8;

    // Push a JSON value onto the Lua stack as the matching Lua type. Scalars map directly;
    // JSON arrays become Lua tables keyed 1..n, JSON objects become string-keyed tables. Anything
    // deeper than MAX_CONVERSION_DEPTH, or that cannot be represented, becomes nil.
    void pushVarToLua(lua_State* L, const juce::var& v, int depth = 0)
    {
        if (depth > MAX_CONVERSION_DEPTH || ! lua_checkstack(L, 4))
        {
            lua_pushnil(L);
            return;
        }

        if (v.isBool())                                   lua_pushboolean(L, ((bool) v) ? 1 : 0);
        else if (v.isInt() || v.isInt64() || v.isDouble()) lua_pushnumber(L, (double) v);
        else if (v.isString())                            lua_pushstring(L, v.toString().toRawUTF8());
        else if (const juce::Array<juce::var>* a = v.getArray())
        {
            lua_createtable(L, a->size(), 0);
            for (int i = 0; i < a->size(); ++i)
            {
                pushVarToLua(L, (*a)[i], depth + 1);
                lua_rawseti(L, -2, i + 1); // Lua arrays are 1-based
            }
        }
        else if (juce::DynamicObject* o = v.getDynamicObject())
        {
            const juce::NamedValueSet& props = o->getProperties();
            lua_createtable(L, 0, props.size());
            for (int i = 0; i < props.size(); ++i)
            {
                lua_pushstring(L, props.getName(i).toString().toRawUTF8());
                pushVarToLua(L, props.getValueAt(i), depth + 1);
                lua_rawset(L, -3);
            }
        }
        else lua_pushnil(L);
    }

    // Convert the Lua value at stack index idx into a juce::var for the report.
    //
    // Tables become a JSON array when their keys are exactly the contiguous integers 1..n (the
    // Lua list convention), and a JSON object with stringified keys otherwise. Values that have
    // no JSON equivalent are reported as a marker string rather than silently dropped, so a test
    // asserting on a returned table can tell "the field was a function" from "the field was nil".
    // `visited` holds the table pointers on the current path: revisiting one is a cycle, but a
    // table referenced twice in sibling positions still serialises fully (it is erased on exit).
    juce::var luaToVar(lua_State* L, int idx, int depth, std::set<const void*>& visited)
    {
        switch (lua_type(L, idx))
        {
            case LUA_TBOOLEAN: return juce::var((bool) lua_toboolean(L, idx));
            case LUA_TNUMBER:  return juce::var((double) lua_tonumber(L, idx));
            case LUA_TSTRING:  return juce::var(juce::String::fromUTF8(lua_tostring(L, idx)));
            case LUA_TFUNCTION: return juce::var("<function>");
            case LUA_TUSERDATA:
            case LUA_TLIGHTUSERDATA: return juce::var("<userdata>");
            case LUA_TTHREAD:  return juce::var("<thread>");
            case LUA_TTABLE:   break; // handled below
            default:           return juce::var(); // nil / unsupported
        }

        if (depth >= MAX_CONVERSION_DEPTH)
            return juce::var("<max-depth>");
        if (! lua_checkstack(L, 4))
            return juce::var("<max-depth>");

        // lua_next below pushes onto the stack, so a relative index would drift; pin it to an
        // absolute one. (Lua 5.1 has no lua_absindex; pseudo-indices are already absolute.)
        const int t = (idx < 0 && idx > LUA_REGISTRYINDEX) ? lua_gettop(L) + idx + 1 : idx;
        const void* ptr = lua_topointer(L, t);
        if (! visited.insert(ptr).second)
            return juce::var("<cycle>");

        // First pass: count the keys, so a table whose keys are exactly 1..n can be emitted as a
        // JSON array. lua_objlen gives n for the array part; if that accounts for every key, it
        // is a list.
        const int arrayLen = (int) lua_objlen(L, t);
        int keyCount = 0;
        lua_pushnil(L);
        while (lua_next(L, t) != 0)
        {
            ++keyCount;
            lua_pop(L, 1); // drop value, keep key for the next iteration
        }
        const bool isList = (arrayLen > 0 && keyCount == arrayLen);

        juce::var out;
        if (isList)
        {
            juce::Array<juce::var> items;
            for (int i = 1; i <= arrayLen; ++i)
            {
                lua_rawgeti(L, t, i);
                items.add(luaToVar(L, -1, depth + 1, visited));
                lua_pop(L, 1);
            }
            out = juce::var(items);
        }
        else
        {
            out = probeReport::makeObject();
            lua_pushnil(L);
            while (lua_next(L, t) != 0)
            {
                // lua_tostring on the *key* would coerce a number key in place and confuse
                // lua_next, so copy the key before stringifying it.
                lua_pushvalue(L, -2);
                const char* rawKey = lua_tostring(L, -1);
                juce::String key = (rawKey != nullptr) ? juce::String::fromUTF8(rawKey)
                                                       : juce::String("<non-scalar-key>");
                lua_pop(L, 1);
                if (key.isEmpty())
                    key = "<empty-key>"; // juce::Identifier cannot be constructed from ""
                probeReport::setProp(out, key, luaToVar(L, -1, depth + 1, visited));
                lua_pop(L, 1); // drop value, keep key
            }
        }

        visited.erase(ptr);
        return out;
    }

    // Convenience overload: convert the value at idx with a fresh cycle set.
    juce::var luaToVar(lua_State* L, int idx)
    {
        std::set<const void*> visited;
        return luaToVar(L, idx, 0, visited);
    }

    // Lua truthiness applied to a converted value: only nil and false are false, so 0 and "" are
    // both truthy (unlike C or Python). luaToVar maps Lua nil to a void var.
    bool isLuaTruthy(const juce::var& v)
    {
        if (v.isVoid())
            return false;
        if (v.isBool())
            return (bool) v;
        return true;
    }

    // pcall a function already sitting on the stack with `nargs` arguments pushed above it,
    // collecting exactly one return value into resultOut. Leaves the stack as it found it either
    // way. Returns false and fills errorOut if the call raised.
    bool pcallForResult(lua_State* L, int nargs, juce::var& resultOut, juce::String& errorOut)
    {
        if (lua_pcall(L, nargs, 1, 0) != 0)
        {
            const char* msg = lua_tostring(L, -1);
            errorOut = (msg != nullptr) ? juce::String::fromUTF8(msg)
                                        : juce::String("(no error message)");
            lua_pop(L, 1);
            return false;
        }
        resultOut = luaToVar(L, -1);
        lua_pop(L, 1);
        return true;
    }

    // Run a chunk of Lua source in the panel's own lua_State and collect its single result.
    //
    // The chunk is tried as an *expression* first ("return <chunk>"), exactly as the stock Lua
    // REPL does, so `codec.get(img, 'a.b')` yields its return value instead of being executed as
    // a bare call statement that throws the result away. Only if that fails to compile is the
    // chunk run verbatim, which is what makes multi-statement chunks ("local x = 1 return x * 2")
    // work too.
    bool evalLuaChunk(lua_State* L, const juce::String& chunk,
                      juce::var& resultOut, juce::String& errorOut)
    {
        const juce::String asExpression = "return " + chunk;
        if (luaL_loadstring(L, asExpression.toRawUTF8()) != 0)
        {
            lua_pop(L, 1); // discard the expression-form error; the statement form has the say
            if (luaL_loadstring(L, chunk.toRawUTF8()) != 0)
            {
                const char* msg = lua_tostring(L, -1);
                errorOut = "compile error: "
                           + ((msg != nullptr) ? juce::String::fromUTF8(msg) : juce::String("?"));
                lua_pop(L, 1);
                return false;
            }
        }
        return pcallForResult(L, 0, resultOut, errorOut);
    }

    // Build a synthetic mouse event centred on the component. CtrlrComponent::mouseDown/mouseUp
    // only forward it to the Lua mouse callbacks (which ignore its geometry), so default
    // pressure/orientation/tilt are fine.
    juce::MouseEvent makeClickEvent(juce::Component* comp)
    {
        const juce::Point<float> centre((float) comp->getWidth() * 0.5f, (float) comp->getHeight() * 0.5f);
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),
                                centre, juce::ModifierKeys(),
                                juce::MouseInputSource::invalidPressure,
                                juce::MouseInputSource::invalidOrientation,
                                juce::MouseInputSource::invalidRotation,
                                juce::MouseInputSource::invalidTiltX,
                                juce::MouseInputSource::invalidTiltY,
                                comp, comp,
                                juce::Time::getCurrentTime(),
                                centre, juce::Time::getCurrentTime(),
                                1, false);
    }

    // Write a var as one JSON document to the real (saved) stdout. `compact` puts it on a single
    // line, which is what --stdin's one-report-per-line framing needs.
    void writeReport(const juce::var& report, bool compact)
    {
        const juce::String json = juce::JSON::toString(report, compact) + "\n";
        const int fd = g_realStdout >= 0 ? g_realStdout : probe_fd::stdoutFd;
        const char* bytes = json.toRawUTF8();
        size_t remaining = std::strlen(bytes);
        while (remaining > 0)
        {
            const long written = probe_fd::write(fd, bytes, remaining);
            if (written <= 0)
                break;
            bytes += written;
            remaining -= (size_t) written;
        }
    }

    // Print a var as JSON, then hard-exit. Clean teardown is deliberately skipped: the panel/Lua
    // object graph is known to leak once JUCE shuts down and the Debug leak detector would fire
    // after we have already produced the report. A one-shot tool just exits.
    [[noreturn]] void emitAndExit(const juce::var& report, int code)
    {
        writeReport(report, false);
        std::_Exit(code);
    }

    // Minimal error object for the load/parse failure path (exit 2).
    [[noreturn]] void failFatal(const juce::String& message, const juce::String& detail = {})
    {
        juce::var err = probeReport::makeObject();
        probeReport::setProp(err, "reportVersion", PROBE_REPORT_VERSION);
        probeReport::setProp(err, "error", message);
        if (detail.isNotEmpty())
            probeReport::setProp(err, "detail", detail);
        probeReport::setProp(err, "exitCode", 2);
        emitAndExit(err, 2);
    }

    // Pick the mock device to rebind to. The mock exposes one device with two ports, enumerated by
    // CtrlrX as "Device 00, port 00" / "Device 00, port 01". Fixture panels use port 00 for input
    // and port 01 for output, so mirror that when both are present; otherwise fall back to ends.
    juce::String pickMockDevice(const juce::StringArray& list, bool wantOutput)
    {
        if (list.isEmpty())
            return {};
        const juce::String suffix = wantOutput ? "port 01" : "port 00";
        for (const auto& s : list)
            if (s.endsWith(suffix))
                return s;
        return wantOutput ? list[list.size() - 1] : list[0];
    }

    // Everything a step needs to drive the loaded panel.
    struct RunContext
    {
        CtrlrProcessor& processor;
        CtrlrPanel& panel;
        MockMidi& midi;
        juce::AudioSampleBuffer& buffer;
        std::mutex& sendsMutex;
        std::vector<juce::MidiMessage>& deviceSends;
        std::set<juce::String>& knownInvalidMethods; // methods already invalid; used to detect new runtime errors
        LogCapture& log;
        int settleMs = DEFAULT_SETTLE_MS; // ceiling: give up waiting for quiet after this long
        int quietMs  = DEFAULT_QUIET_MS;  // stop as soon as nothing has happened for this long
    };

    // Parse a whitespace/comma-separated hex byte string ("F0 7E 41 F7") into a MidiMessage.
    // Returns false on an empty/garbage string.
    bool parseHexBytes(const juce::String& hex, juce::MidiMessage& out)
    {
        juce::StringArray tokens;
        tokens.addTokens(hex, " \t\r\n,", "");
        tokens.removeEmptyStrings();
        juce::MemoryBlock bytes;
        for (const auto& tok : tokens)
        {
            const juce::String t = tok.startsWithIgnoreCase("0x") ? tok.substring(2) : tok;
            if (t.isEmpty() || ! t.containsOnly("0123456789abcdefABCDEF") || t.length() > 2)
                return false;
            const juce::uint8 b = (juce::uint8) t.getHexValue32();
            bytes.append(&b, 1);
        }
        if (bytes.getSize() == 0)
            return false;
        out = juce::MidiMessage(bytes.getData(), (int) bytes.getSize());
        return true;
    }

    // Every failure a step can produce goes into one array of {type, message} objects, so a
    // caller never has to know which of several channels a given failure would have landed in.
    // `type` is "action" (the probe could not carry out what was asked) or "lua" (the panel's
    // own Lua raised).
    void addError(juce::Array<juce::var>& errors, const char* type, const juce::String& message)
    {
        juce::var e = probeReport::makeObject();
        probeReport::setProp(e, "type", type);
        probeReport::setProp(e, "message", message);
        errors.add(e);
    }

    // Count the entries of one type in a step's errors array.
    int countErrorsOfType(const juce::var& errors, const juce::String& type)
    {
        int n = 0;
        if (const juce::Array<juce::var>* a = errors.getArray())
            for (const auto& e : *a)
                if (e.getProperty("type", juce::var()).toString() == type)
                    ++n;
        return n;
    }

    // Append, to `errors`, any method that transitioned valid -> invalid since we last looked
    // (CATCH_METHOD_EXCEPTION marks a method invalid when its Lua raises at runtime). The actual
    // "Callback error: ..." text is only logged (level LuaError), so we fold in whatever error
    // text the LogCapture observed during this step instead of the old "see stderr" placeholder.
    void collectNewLuaErrors(RunContext& ctx, juce::Array<juce::var>& errors,
                             const juce::StringArray& capturedErrorText)
    {
        const juce::String detail = capturedErrorText.isEmpty()
            ? juce::String("see stderr log for the Lua message (run with --verbose)")
            : capturedErrorText.joinIntoString(" | ");

        CtrlrLuaMethodManager& mgr = ctx.panel.getCtrlrLuaManager().getMethodManager();
        for (int i = 0; i < mgr.getNumMethods(); ++i)
        {
            CtrlrLuaMethod* method = mgr.getMethodByIndex(i);
            if (method == nullptr || method->isValid())
                continue;
            const juce::String name = method->getName();
            if (ctx.knownInvalidMethods.insert(name).second) // newly invalid
                addError(errors, "lua",
                         "method '" + name + "' raised a runtime error and was disabled: " + detail);
        }
    }

    // Drain whatever the device-send recorder has captured so far into deviceMidiOut.
    // Returns how many messages were taken. Device sends arrive on JUCE's MidiOutput background
    // thread, hence the lock.
    int takeDeviceSends(RunContext& ctx, juce::Array<juce::var>& deviceMidiOut)
    {
        std::lock_guard<std::mutex> l(ctx.sendsMutex);
        const int n = (int) ctx.deviceSends.size();
        for (const auto& m : ctx.deviceSends)
            deviceMidiOut.add(probeReport::midiToHex(m));
        ctx.deviceSends.clear();
        return n;
    }

    // Implicit settle after every action, driven by a quiet window rather than a fixed sleep.
    //
    // Each ~5 ms iteration pumps the message loop (which is what delivers CtrlrLog's AsyncUpdater
    // and the async modulator callbacks) and runs one processBlock (which flushes host output and
    // the panel's MIDI collector), then drains the device-send recorder. Anything new -- a device
    // send, host MIDI, or a log line -- resets the quiet timer; the loop returns as soon as
    // quietMs has passed with nothing happening, and gives up at the ceilingMs ceiling.
    //
    // Consequences worth knowing:
    //  - ceilingMs is a *ceiling*. A typical pure-Lua step now costs ~quietMs instead of the full
    //    settle, which is where the bulk of the suite's runtime went.
    //  - ceilingMs == 0 still pumps once and runs one processBlock. A hard bypass would silently
    //    drop console() output and Lua error text, since CtrlrLog only delivers while the message
    //    loop runs -- the same class of trap this rework exists to remove.
    //  - A panel with a chatty repeating timer never settles quiet, so it degrades to the ceiling
    //    cost; it cannot spin here.
    void settle(RunContext& ctx, int ceilingMs, int quietMs,
                juce::Array<juce::var>& deviceMidiOut, juce::Array<juce::var>& hostMidiOut,
                const juce::MidiBuffer& firstBlockInput = {})
    {
        const auto start = std::chrono::steady_clock::now();
        auto lastActivity = start;
        auto elapsedMsSince = [](std::chrono::steady_clock::time_point t) {
            return (int) std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - t).count();
        };

        juce::MidiBuffer midi;
        size_t lastLogSize = ctx.log.entries.size();
        bool first = true;

        for (;;)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(SETTLE_SLICE_MS);

            midi.clear();
            if (first)
                midi.addEvents(firstBlockInput, 0, -1, 0); // carry host-injected input in block 0
            ctx.processor.processBlock(ctx.buffer, midi);

            bool sawActivity = false;
            for (const auto meta : midi)
            {
                hostMidiOut.add(probeReport::midiToHex(meta.getMessage()));
                sawActivity = true;
            }
            if (takeDeviceSends(ctx, deviceMidiOut) > 0)
                sawActivity = true;
            if (ctx.log.entries.size() != lastLogSize)
            {
                lastLogSize = ctx.log.entries.size();
                sawActivity = true;
            }

            const auto now = std::chrono::steady_clock::now();
            if (sawActivity)
                lastActivity = now;

            first = false;
            if (elapsedMsSince(start) >= ceilingMs)
                break;
            if (elapsedMsSince(lastActivity) >= quietMs)
                break;
        }
    }

    // Pump for exactly `ms`, with no early exit: the floor semantics `wait` promises. Still
    // collects everything settle() would, so a late device send is not dropped.
    void waitFixed(RunContext& ctx, int ms,
                   juce::Array<juce::var>& deviceMidiOut, juce::Array<juce::var>& hostMidiOut,
                   const juce::MidiBuffer& firstBlockInput = {})
    {
        // A quiet window as long as the wait itself can never elapse first, so this runs the full
        // duration while reusing the same pump/processBlock/drain loop.
        settle(ctx, ms, ms + 1, deviceMidiOut, hostMidiOut, firstBlockInput);
    }

    // Discard anything the recorder captured so a step's capture starts clean.
    void clearRecorder(RunContext& ctx)
    {
        std::lock_guard<std::mutex> l(ctx.sendsMutex);
        ctx.deviceSends.clear();
    }

    // Recursively find the first juce::Button under a component (its internal button, for click).
    juce::Button* findButtonChild(juce::Component* c)
    {
        for (int i = 0; i < c->getNumChildComponents(); ++i)
        {
            juce::Component* child = c->getChildComponent(i);
            if (auto* b = dynamic_cast<juce::Button*>(child))
                return b;
            if (auto* b = findButtonChild(child))
                return b;
        }
        return nullptr;
    }

    // Dispatch one action, run the settle, and return the completed step var.
    juce::var executeStep(RunContext& ctx, int index, const juce::var& action)
    {
        juce::var step = probeReport::makeObject();
        probeReport::setProp(step, "index", index);
        probeReport::setProp(step, "action", action);

        juce::Array<juce::var> errors; // every failure, action- or Lua-sourced, ends up here
        const juce::String actionName = action.getProperty("action", juce::var()).toString();

        // settleMs / quietMs are read from *every* action, not just `wait`, so a step known to be
        // slow (or known to need no settle at all) can be tuned without changing the whole script.
        const int settleMs = (int) action.getProperty("settleMs", juce::var(ctx.settleMs));
        const int quietMs  = (int) action.getProperty("quietMs",  juce::var(ctx.quietMs));

        bool isFixedWait = false;   // `wait` keeps floor semantics: run the full duration
        int  fixedWaitMs = 0;
        bool settleDone  = false;   // `waitFor` does its own pumping; don't settle again after it
        juce::MidiBuffer hostInput; // only injectHostMidi fills this
        juce::var stepReturn;       // an action that produces a Lua value records it here
        bool hasReturn = false;
        juce::Array<juce::var> deviceMidiOut, hostMidiOut;

        // Console/Lua-error lines captured during this step start after everything logged so far.
        const size_t logStart = ctx.log.entries.size();

        clearRecorder(ctx);

        if (actionName == "wait")
        {
            // Deliberately a floor, not a ceiling: `wait` is how a script says "this panel does
            // something on a timer, give it this long" -- an early quiet-window exit would defeat
            // the point.
            isFixedWait = true;
            fixedWaitMs = (int) action.getProperty("ms", juce::var(settleMs));
        }
        else if (actionName == "setValue")
        {
            const juce::String modName = action.getProperty("modulator", juce::var()).toString();
            CtrlrModulator* mod = ctx.panel.getModulator(modName);
            if (mod == nullptr)
            {
                addError(errors, "action","no modulator named '" + modName + "'");
            }
            else
            {
                const double value = (double) action.getProperty("value", juce::var(0));
                // Prefer the GUI path (setComponentValue -> setValueGeneric(changedByGUI)) so the
                // full knob-move wiring fires; fall back to the processor directly if this headless
                // modulator has no component.
                if (CtrlrComponent* comp = mod->getComponent())
                    comp->setComponentValue(value, true);
                else
                    mod->getProcessor().setValueGeneric(
                        CtrlrModulatorValue(value, CtrlrModulatorValue::changedByGUI), true);
            }
        }
        else if (actionName == "click")
        {
            const juce::String modName = action.getProperty("modulator", juce::var()).toString();
            CtrlrModulator* mod = ctx.panel.getModulator(modName);
            if (mod == nullptr)
                addError(errors, "action","no modulator named '" + modName + "'");
            else if (CtrlrComponent* comp = mod->getComponent())
            {
                // Drive the full component mouse-up path, not just the internal juce::Button:
                //   mouseDown -> (triggerClick) -> mouseUp
                // CtrlrComponent::mouseDown/mouseUp fire componentLuaMouseDown/componentLuaMouseUp
                // (the nav-button wiring); triggerClick() drives buttonClicked -> the value-change /
                // MIDI path. A trigger-on-mouse-down button self-triggers inside CtrlrButton::mouseDown,
                // so we skip the explicit triggerClick() then to avoid a double click.
                const juce::MouseEvent e = makeClickEvent(comp);
                juce::Button* button = findButtonChild(comp);
                const bool triggerOnDown = (bool) mod->getObjectTree()
                    .getChildWithName(Ids::component).getProperty(Ids::uiButtonTriggerOnMouseDown);

                comp->mouseDown(e);
                if (button != nullptr && ! triggerOnDown)
                    button->triggerClick();
                comp->mouseUp(e);
            }
            else
            {
                addError(errors, "action","modulator '" + modName + "' has no component to click");
            }
        }
        else if (actionName == "injectDeviceMidi")
        {
            juce::MidiMessage m;
            if (! parseHexBytes(action.getProperty("bytes", juce::var()).toString(), m))
                addError(errors, "action","injectDeviceMidi: could not parse 'bytes'");
            else
                ctx.midi.injectMidiInput(m); // arrives as if from the (mock) hardware input
        }
        else if (actionName == "injectHostMidi")
        {
            juce::MidiMessage m;
            if (! parseHexBytes(action.getProperty("bytes", juce::var()).toString(), m))
                addError(errors, "action","injectHostMidi: could not parse 'bytes'");
            else
                hostInput.addEvent(m, 0); // carried into the first settle processBlock
        }
        else if (actionName == "callLuaMethod")
        {
            const juce::String methodName = action.getProperty("method", juce::var()).toString();
            const juce::var args = action.getProperty("args", juce::var());
            CtrlrLuaMethod* method = ctx.panel.getCtrlrLuaManager().getMethodManager().getMethodByName(methodName);
            if (method == nullptr)
                addError(errors, "action", "no Lua method named '" + methodName + "'");
            else if (! method->isValid())
                addError(errors, "action", "Lua method '" + methodName + "' is invalid");
            else
            {
                // Always the raw-Lua path, arguments or not. Methods are compiled as globals by
                // name, so we fetch the global, push the args as their Lua types and pcall for one
                // return value. Going through CtrlrLuaMethodManager::call() instead (as this used
                // to for the nullary case) would discard the return value -- which is why callers
                // used to pass a dummy argument just to be able to read one -- and would route a
                // runtime error through CATCH_METHOD_EXCEPTION's modal alert. With pcall the error
                // comes back to us and lands in errors[] as type "lua".
                lua_State* L = ctx.panel.getCtrlrLuaManager().getLuaState();
                lua_getglobal(L, methodName.toRawUTF8());
                if (! lua_isfunction(L, -1))
                {
                    lua_pop(L, 1);
                    addError(errors, "action", "Lua global '" + methodName + "' is not a callable function");
                }
                else
                {
                    const juce::Array<juce::var>* a = args.getArray();
                    const int numArgs = (a != nullptr) ? a->size() : 0;
                    for (int i = 0; i < numArgs; ++i)
                        pushVarToLua(L, (*a)[i]);

                    juce::String err;
                    if (pcallForResult(L, numArgs, stepReturn, err))
                        hasReturn = true;
                    else
                        addError(errors, "lua", "callLuaMethod '" + methodName + "' raised: " + err);
                }
            }
        }
        else if (actionName == "eval")
        {
            // Run an arbitrary Lua chunk in the panel's own lua_State, so panel internals (module
            // tables, upvalue-free globals, anything a method can see) are directly assertable
            // without injecting a throwaway <luaMethod> into a scratch copy of the panel.
            const juce::String chunk = action.getProperty("lua", juce::var()).toString();
            if (chunk.trim().isEmpty())
                addError(errors, "action", "eval: 'lua' is missing or empty");
            else
            {
                juce::String err;
                if (evalLuaChunk(ctx.panel.getCtrlrLuaManager().getLuaState(), chunk, stepReturn, err))
                    hasReturn = true;
                else
                    addError(errors, "lua", "eval failed: " + err);
            }
        }
        else if (actionName == "waitFor")
        {
            // Poll a Lua condition until it goes truthy, pumping the same way settle() does in
            // between. This is how a script waits on something genuinely asynchronous (a dump
            // arriving, a timer firing) without guessing at a `wait` duration.
            const juce::String chunk = action.getProperty("lua", juce::var()).toString();
            const int timeoutMs = (int) action.getProperty("timeoutMs", juce::var(2000));
            const int pollMs = juce::jmax(1, (int) action.getProperty("pollMs", juce::var(25)));

            if (chunk.trim().isEmpty())
            {
                addError(errors, "action", "waitFor: 'lua' is missing or empty");
            }
            else
            {
                lua_State* L = ctx.panel.getCtrlrLuaManager().getLuaState();
                const auto start = std::chrono::steady_clock::now();
                bool satisfied = false, chunkFailed = false;

                for (;;)
                {
                    waitFixed(ctx, pollMs, deviceMidiOut, hostMidiOut, hostInput);
                    hostInput.clear(); // injected input rides only the first block

                    juce::String err;
                    juce::var result;
                    if (! evalLuaChunk(L, chunk, result, err))
                    {
                        addError(errors, "lua", "waitFor condition failed: " + err);
                        chunkFailed = true;
                        break;
                    }
                    stepReturn = result;
                    hasReturn = true;
                    if (isLuaTruthy(result))
                    {
                        satisfied = true;
                        break;
                    }
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start).count() >= timeoutMs)
                        break;
                }

                if (! satisfied && ! chunkFailed)
                    addError(errors, "action", "waitFor timed out after " + juce::String(timeoutMs)
                                               + " ms waiting for: " + chunk);
            }
            settleDone = true; // the poll loop already pumped and collected
        }
        else if (actionName == "screenshot")
        {
            // Render the live canvas to a PNG. The canvas never had a desktop peer, but it is
            // fully built and laid out, so createComponentSnapshot paints it the same way the
            // editor window would.
            const juce::String path = action.getProperty("file", juce::var()).toString();
            CtrlrPanelCanvas* canvas = ctx.panel.getCanvas();
            if (path.isEmpty())
                addError(errors, "action", "screenshot: 'file' is missing");
            else if (canvas == nullptr)
                addError(errors, "action", "screenshot: the panel has no canvas");
            else
            {
                // The canvas adds an always-on-top ResizableBorderComponent child; leaving it
                // visible would draw a resize frame around every screenshot.
                juce::Component* border = canvas->getResizableBorder();
                const bool borderWasVisible = (border != nullptr) && border->isVisible();
                if (borderWasVisible)
                    border->setVisible(false);

                const juce::Image image = canvas->createComponentSnapshot(canvas->getLocalBounds(), true);

                if (borderWasVisible)
                    border->setVisible(true);

                const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(path);
                out.deleteFile();
                std::unique_ptr<juce::FileOutputStream> stream(out.createOutputStream());
                juce::PNGImageFormat png;
                if (stream == nullptr || ! png.writeImageToStream(image, *stream))
                {
                    addError(errors, "action", "screenshot: could not write " + out.getFullPathName());
                }
                else
                {
                    stream->flush();
                    juce::var shot = probeReport::makeObject();
                    probeReport::setProp(shot, "file", out.getFullPathName());
                    probeReport::setProp(shot, "width", image.getWidth());
                    probeReport::setProp(shot, "height", image.getHeight());
                    probeReport::setProp(step, "screenshot", shot);
                }
            }
        }
        else if (actionName == "setLayerVisible")
        {
            // Set a canvas layer's visibility (layers are pages). Headless there is no live layer
            // Component, so this sets the uiPanelCanvasLayerVisibility tree property directly, which
            // is what the report reads back. Match by uid first, then by name.
            const juce::String layerId = action.getProperty("layer", juce::var()).toString();
            const bool visible = (bool) action.getProperty("visible", juce::var(true));
            const juce::ValueTree editor = ctx.panel.getObjectTree().getChildWithName(Ids::uiPanelEditor);

            juce::ValueTree target;
            for (int pass = 0; pass < 2 && ! target.isValid(); ++pass)
            {
                const juce::Identifier key = (pass == 0) ? Ids::uiPanelCanvasLayerUid : Ids::uiPanelCanvasLayerName;
                for (int i = 0; i < editor.getNumChildren(); ++i)
                {
                    const juce::ValueTree layer = editor.getChild(i);
                    if (layer.hasType(Ids::uiPanelCanvasLayer)
                        && layer.getProperty(key).toString() == layerId)
                    {
                        target = layer;
                        break;
                    }
                }
            }

            if (target.isValid())
                target.setProperty(Ids::uiPanelCanvasLayerVisibility, visible, nullptr);
            else
                addError(errors, "action","no canvas layer with uid or name '" + layerId + "'");
        }
        else
        {
            addError(errors, "action","unknown action '" + actionName + "'");
        }

        if (! settleDone)
        {
            if (isFixedWait)
                waitFixed(ctx, fixedWaitMs, deviceMidiOut, hostMidiOut, hostInput);
            else
                settle(ctx, settleMs, quietMs, deviceMidiOut, hostMidiOut, hostInput);
        }

        // Slice the log lines captured during this step (delivered on the message thread inside
        // settle): console() output (level Lua) and the real Lua error text (level LuaError).
        juce::Array<juce::var> console;
        juce::StringArray capturedErrorText;
        for (size_t i = logStart; i < ctx.log.entries.size(); ++i)
        {
            const LogCapture::Entry& e = ctx.log.entries[i];
            if (e.level == CtrlrLog::Lua)
                console.add(e.message);
            else if (e.level == CtrlrLog::LuaError || e.level == CtrlrLog::LuaRuntimeError)
                capturedErrorText.add(e.message);
        }

        const int errorsBeforeCollect = errors.size();
        collectNewLuaErrors(ctx, errors, capturedErrorText);
        // Surface captured error text even if no method flipped to invalid (e.g. an error raised
        // from a callback that did not disable a named method), so it is never silently dropped.
        if (errors.size() == errorsBeforeCollect)
            for (const auto& line : capturedErrorText)
                addError(errors, "lua", line);

        probeReport::setProp(step, "deviceMidiOut", deviceMidiOut);
        probeReport::setProp(step, "hostMidiOut", hostMidiOut);
        probeReport::setProp(step, "errors", errors);
        probeReport::setProp(step, "console", console);
        if (hasReturn)
            probeReport::setProp(step, "returnValue", stepReturn);
        probeReport::setProp(step, "ok", errors.isEmpty());
        return step;
    }

    // ------------------------------------------------------------------------------------------
    // Session: the long-lived process state, so one process can run several scripts (--stdin).
    // ------------------------------------------------------------------------------------------
    struct Session
    {
        CtrlrProcessor& processor;
        MockMidi& midi;
        juce::AudioSampleBuffer& buffer;
        std::mutex& sendsMutex;
        std::vector<juce::MidiMessage>& deviceSends;
        LogCapture& log;
        juce::File panelFile;

        // Rebuilt on every (re)load:
        CtrlrPanel* panel = nullptr;
        juce::String origIn, origOut;
        juce::var compileErrors;
        std::set<juce::String> knownInvalidMethods;
    };

    // Take the panel the manager just loaded, rebind its MIDI to the mock devices and capture the
    // Lua baselines. Returns false (with a reason) if no panel is there.
    //
    // The compile-error baseline has to be taken *before* any step runs: a runtime Lua error marks
    // its method invalid, so reading it afterwards would misreport runtime failures as compile
    // errors. knownInvalidMethods is seeded from the same baseline so per-step detection only
    // reports methods that go invalid during a step.
    bool bindLoadedPanel(Session& s, juce::String& errorOut)
    {
        CtrlrManager& manager = s.processor.getManager();
        if (manager.getNumPanels() <= 0)
        {
            errorOut = "no panel was loaded";
            return false;
        }
        s.panel = manager.getPanel(manager.getNumPanels() - 1);
        if (s.panel == nullptr)
        {
            errorOut = "the loaded panel was null";
            return false;
        }

        // Record the panel's original device bindings, then rebind to the mock devices so the
        // panel's MIDI I/O is captured. The file on disk is never touched.
        s.origIn  = s.panel->getProperty(Ids::panelMidiInputDevice).toString();
        s.origOut = s.panel->getProperty(Ids::panelMidiOutputDevice).toString();

        CtrlrMIDIDeviceManager& devMgr = manager.getCtrlrMIDIDeviceManager();
        const juce::String mockIn  = pickMockDevice(devMgr.getManagedDevices(inputDevice),  false);
        const juce::String mockOut = pickMockDevice(devMgr.getManagedDevices(outputDevice), true);
        if (mockIn.isNotEmpty())
            s.panel->setProperty(Ids::panelMidiInputDevice, mockIn, false);
        if (mockOut.isNotEmpty())
            s.panel->setProperty(Ids::panelMidiOutputDevice, mockOut, false);

        // Let the rebind settle (opens the mock devices / starts the input thread).
        s.processor.prepareToPlay(44100, BLOCK_SIZE);
        for (int i = 0; i < 12; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(25);

        s.compileErrors = probeReport::luaCompileErrors(*s.panel);
        s.knownInvalidMethods.clear();
        CtrlrLuaMethodManager& mgr = s.panel->getCtrlrLuaManager().getMethodManager();
        for (int i = 0; i < mgr.getNumMethods(); ++i)
            if (CtrlrLuaMethod* m = mgr.getMethodByIndex(i))
                if (! m->isValid())
                    s.knownInvalidMethods.insert(m->getName());

        return true;
    }

    // Open the panel file and bind it. Returns false (with a reason) on failure.
    bool openPanel(Session& s, juce::String& errorOut)
    {
        try
        {
            s.processor.openFileFromCli(s.panelFile);
        }
        catch (...)
        {
            errorOut = "opening the panel threw";
            return false;
        }
        return bindLoadedPanel(s, errorOut);
    }

    // Run one script object against the bound panel and return the finished report.
    // `logStart` is where this script's console output begins in the shared log capture.
    juce::var runScript(Session& s, const juce::var& script, bool globalConsole, size_t logStart)
    {
        RunContext ctx{ s.processor, *s.panel, s.midi, s.buffer, s.sendsMutex, s.deviceSends,
                        s.knownInvalidMethods, s.log,
                        (int) script.getProperty("settleMs", juce::var(DEFAULT_SETTLE_MS)),
                        (int) script.getProperty("quietMs",  juce::var(DEFAULT_QUIET_MS)) };

        juce::Array<juce::var> stepResults;
        int actionErrorCount = 0;     // the probe could not carry out what a step asked for
        int luaRuntimeErrorCount = 0; // the panel's Lua raised during a step
        const juce::var steps = script.getProperty("steps", juce::var());
        if (const juce::Array<juce::var>* stepArray = steps.getArray())
        {
            for (int i = 0; i < stepArray->size(); ++i)
            {
                const juce::var stepResult = executeStep(ctx, i, (*stepArray)[i]);
                const juce::var stepErrors = stepResult.getProperty("errors", juce::var());
                actionErrorCount     += countErrorsOfType(stepErrors, "action");
                luaRuntimeErrorCount += countErrorsOfType(stepErrors, "lua");
                stepResults.add(stepResult);
            }
        }

        juce::var report = probeReport::makeObject();
        probeReport::setProp(report, "reportVersion", PROBE_REPORT_VERSION);

        juce::var panelInfo = probeReport::makeObject();
        probeReport::setProp(panelInfo, "file", s.panelFile.getFullPathName());
        probeReport::setProp(panelInfo, "name", s.panel->getName());
        juce::var origDevices = probeReport::makeObject();
        probeReport::setProp(origDevices, "input", s.origIn);
        probeReport::setProp(origDevices, "output", s.origOut);
        probeReport::setProp(panelInfo, "originalDevices", origDevices);
        probeReport::setProp(panelInfo, "luaCompileErrors", s.compileErrors);
        probeReport::setProp(report, "panel", panelInfo);

        probeReport::setProp(report, "steps", stepResults);
        probeReport::setProp(report, "finalState", probeReport::finalState(*s.panel));

        // Global console aggregate (opt-in): every console() line this script produced. Gated
        // behind a flag because a chatty panel can emit a lot; the per-step console arrays are
        // always present.
        if (globalConsole)
        {
            juce::Array<juce::var> allConsole;
            for (size_t i = logStart; i < s.log.entries.size(); ++i)
                if (s.log.entries[i].level == CtrlrLog::Lua)
                    allConsole.add(s.log.entries[i].message);
            probeReport::setProp(report, "console", allConsole);
        }

        const int compileErrorCount = s.compileErrors.isArray() ? s.compileErrors.size() : 0;
        // errorCount is the total of all three channels, so a caller that asserts on a single
        // number cannot get a green run while something failed in a channel it forgot to check.
        const int errorCount = compileErrorCount + luaRuntimeErrorCount + actionErrorCount;

        juce::var summary = probeReport::makeObject();
        probeReport::setProp(summary, "errorCount", errorCount); // compile + runtime + action
        probeReport::setProp(summary, "actionErrorCount", actionErrorCount);
        probeReport::setProp(summary, "luaRuntimeErrorCount", luaRuntimeErrorCount);
        probeReport::setProp(summary, "luaCompileErrorCount", compileErrorCount);
        probeReport::setProp(summary, "exitCode", (errorCount > 0) ? 1 : 0);
        probeReport::setProp(report, "summary", summary);

        return report;
    }
}

int main(int argc, char** argv)
{
    // Isolate stdout up front: from here on only JSON reports reach the real stdout.
    redirectStdoutToStderr();

    // InitGoogleMock initialises gmock (needed for NiceMock/ON_CALL) and strips its own flags,
    // leaving our positional arguments in place.
    ::testing::InitGoogleMock(&argc, argv);

    // Positional args: <panel-file> [<script-file>]. Flags: --verbose (show stderr chatter),
    // --global-console (add a per-script aggregate of console() output to the report),
    // --stdin (persistent mode: one script object per input line, one report object per output
    // line, so a suite pays the ~0.9 s process startup once instead of per test),
    juce::StringArray positional;
    bool verbose = false;
    bool globalConsole = false;
    bool stdinMode = false;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a(argv[i]);
        if (a == "--verbose")             verbose = true;
        else if (a == "--global-console") globalConsole = true;
        else if (a == "--stdin")          stdinMode = true;
        else if (! a.startsWith("-"))     positional.add(a);
    }
    if (positional.isEmpty())
        failFatal("usage: CtrlrPanelProbe [--verbose] [--global-console] [--stdin] "
                  "<panel-file> [<script-file>]",
                  "CtrlrPanelProbe drives a CtrlrX panel from a JSON script and reports what it did "
                  "-- MIDI out, Lua errors, final state. No hardware, no display. See Doc/PanelProbe.md.");

    // Quiet by default: silence all stderr chatter so stdout stays the clean JSON contract. The
    // reports are written to the saved real-stdout fd, so exit codes and output are unaffected.
    if (! verbose)
        silenceStdErr();


    const juce::File panelFile = juce::File::getCurrentWorkingDirectory().getChildFile(positional[0]);
    if (! panelFile.existsAsFile())
        failFatal("panel file not found", panelFile.getFullPathName());

    // One-shot mode's optional action script: {"settleMs": N, "steps": [ {action...}, ... ]}. An
    // absent or empty script means "load, rebind, snapshot" only. Malformed JSON is a fatal exit-2.
    juce::var script;
    if (positional.size() >= 2)
    {
        const juce::File scriptFile = juce::File::getCurrentWorkingDirectory().getChildFile(positional[1]);
        if (! scriptFile.exists())
            failFatal("script file not found", scriptFile.getFullPathName());
        const juce::String scriptText = scriptFile.loadFileAsString();
        if (scriptText.trim().isNotEmpty())
        {
            const juce::Result parseResult = juce::JSON::parse(scriptText, script);
            if (parseResult.failed())
                failFatal("script parse error", parseResult.getErrorMessage());
        }
    }

    juce::ScopedJuceInitialiser_GUI juceGui;
    juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();

    // Suppress the modal "Callback error" dialogs CtrlrX raises on Lua runtime errors (see above).
    // Force JUCE's own (ModalComponentManager-tracked) alert windows so the suppressor can dismiss
    // them, rather than an untracked native/GTK box.
    juce::LookAndFeel::getDefaultLookAndFeel().setUsingNativeAlertWindows(false);
    ModalDialogSuppressor dialogSuppressor;

    // Reuse the test mock MIDI subsystem. NiceMock (not StrictMock) because there is no gtest
    // context to verify EXPECT_CALLs against; ON_CALL supplies the behaviour the mock needs.
    NiceMock<MockMidi> midi;
    std::mutex sendsMutex;
    std::vector<juce::MidiMessage> deviceSends; // written from JUCE's MidiOutput background thread

    ON_CALL(midi, getNumDevices()).WillByDefault(Return(1));
    ON_CALL(midi, getNumPorts(_)).WillByDefault(Return(2));
    ON_CALL(midi, sendMidiEvent(_, _, _))
        .WillByDefault(Invoke([&](int, int, juce::MidiMessage m) {
            std::lock_guard<std::mutex> l(sendsMutex);
            deviceSends.push_back(m);
        }));

    auto processor = std::make_shared<CtrlrProcessor>();

    // Capture console() output and real Lua error text into the report. Registered before the
    // panel loads so load-time console() lines land in the aggregate too.
    LogCapture logCapture;
    processor->getManager().getCtrlrLog().addListener(&logCapture);

    // JUCE 8 enumerates MIDI devices asynchronously (see test_ProcessorFixture.h): pump the message
    // loop until the mock devices appear, then refresh CtrlrX's device list.
    if (midi.hasSubsystemMock())
    {
        for (int i = 0; i < 40 && juce::MidiOutput::getAvailableDevices().isEmpty(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
        processor->getManager().getCtrlrMIDIDeviceManager().refreshDevices();
    }

    juce::AudioSampleBuffer buffer(2, BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        buffer.setSample(0, i, 1.0f * i);
        buffer.setSample(1, i, 2.0f * i);
    }

    Session session{ *processor, midi, buffer, sendsMutex, deviceSends, logCapture, panelFile };

    juce::String loadError;
    if (! openPanel(session, loadError))
        failFatal("panel failed to load", panelFile.getFullPathName() + ": " + loadError);

    if (! stdinMode)
    {
        const juce::var report = runScript(session, script, globalConsole, 0);
        emitAndExit(report, (int) report.getProperty("summary", juce::var())
                                       .getProperty("exitCode", juce::var(0)));
    }

    // --- Persistent mode ----------------------------------------------------------------------
    // One JSON script object per input line -> one compact JSON report object per output line.
    // EOF exits 0. This exists to amortise the ~1.5 s of process startup (JUCE init, MIDI device
    // enumeration, panel parse) across a whole suite: measured on a large real-world panel, 50 scripts
    // in one process take 1.8 s total against ~75 s of respawning.
    //
    // IMPORTANT: scripts in one process SHARE PANEL STATE -- modulator values, Lua globals, the
    // lot. Isolation is the client's job, and the way to get it is to respawn the process.
    //
    // There is deliberately no in-process `reload` action. It was built and measured: tearing the
    // panel down via CtrlrManager::removePanel and re-opening costs ~1.74 s (0.6-1.1 s of teardown
    // plus ~1.0 s to re-open) and grows RSS by ~0.8 MB a time on the known-leaky teardown path --
    // so it is both slower than simply respawning the process (1.50 s) and leakier. If
    // CtrlrManager ever grows a non-UI panel-teardown path, this becomes worth revisiting.
    std::string line;
    while (std::getline(std::cin, line))
    {
        const juce::String text = juce::String::fromUTF8(line.c_str(), (int) line.size()).trim();
        if (text.isEmpty())
            continue;

        juce::var lineScript;
        const juce::Result parseResult = juce::JSON::parse(text, lineScript);
        if (parseResult.failed())
        {
            // A bad line is the client's bug, not a broken panel: report it and keep going.
            juce::var err = probeReport::makeObject();
            probeReport::setProp(err, "reportVersion", PROBE_REPORT_VERSION);
            probeReport::setProp(err, "error", "script parse error");
            probeReport::setProp(err, "detail", parseResult.getErrorMessage());
            writeReport(err, true);
            continue;
        }

        const size_t logStart = logCapture.entries.size();
        writeReport(runScript(session, lineScript, globalConsole, logStart), true);
    }

    // Same reasoning as emitAndExit: skip teardown rather than trip the leak detector on a
    // known-leaky shutdown path after the reports are already out.
    std::_Exit(0);
}
