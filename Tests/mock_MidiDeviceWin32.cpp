#include "mock_MidiDevice.h"
#if JUCE_WINDOWS

InformMockMidiOfSubsystem mockMidiSubsystem;

#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

/**
 * Windows (winmm) MIDI subsystem mock for TestsProcessor, sibling of mock_MidiDeviceAlsa.cpp.
 *
 * On Linux the ALSA symbols are resolved from libasound *dynamically*, so a local definition
 * shadows the real one. On Windows the winmm `midiIn*`/`midiOut*` functions are pulled in from
 * winmm.dll via the import address table (IAT) -- MSVC binds them through `__imp_*` thunks (see
 * the `#pragma comment(lib, "winmm.lib")` in juce_BasicNativeHeaders.h). A local C definition
 * therefore does NOT shadow them. Instead, at static-init time (before JUCE lazily creates its
 * MidiService singleton) we walk this executable's PE import table and repoint winmm's thunks at
 * the mock functions below, so JUCE's genuine juce_win32_Midi.cpp (Win32MidiService) runs against
 * a fake winmm. winmm stays linked (timers etc. still use it); only the MIDI entry points move.
 *
 * Endpoint numbering matches the ALSA mock exactly: a flattened list of
 * getNumDevices()*getNumPorts() endpoints named "Device %02d, port %02d", with
 * flat index <-> (idx / numPorts, idx % numPorts). Devices are enumerated by winmm device id
 * (the flat index), and midiIn/OutMessage(DRV_QUERYDEVICEINTERFACESIZE) returns
 * MMSYSERR_NOTSUPPORTED so JUCE uses the device name as its identifier (matching fixture lookup).
 */

#ifndef DRV_RESERVED
 #define DRV_RESERVED 0x0800
#endif
#ifndef DRV_QUERYDEVICEINTERFACESIZE
 #define DRV_QUERYDEVICEINTERFACESIZE (DRV_RESERVED + 13)
#endif

namespace
{
    // ---- flattened endpoint numbering (identical to the ALSA mock) -------------------------
    int mockNumDevices()
    {
        MockMidi* m = MockMidi::getInstance();
        return m != nullptr ? m->getNumDevices() : 0;
    }
    int mockNumPorts()
    {
        MockMidi* m = MockMidi::getInstance();
        return m != nullptr ? m->getNumPorts(0) : 0;
    }
    int mockNumEndpoints() { return mockNumDevices() * mockNumPorts(); }

    // winmm MidiInProc: void CALLBACK (HMIDIIN, UINT, DWORD_PTR dwInstance, DWORD_PTR, DWORD_PTR)
    using MidiInProcT = void (CALLBACK*) (HMIDIIN, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

    struct OpenInput
    {
        MidiInProcT callback = nullptr;
        DWORD_PTR   instance = 0;
        bool        started  = false;
        int         dev = 0, port = 0;
        std::deque<MIDIHDR*> buffers; // sysex input buffers added via midiInAddBuffer
    };

    struct OpenOutput { int dev = 0, port = 0; };

    std::mutex g_mutex;                         // guards the registries below
    std::map<HMIDIIN,  OpenInput>  g_inputs;
    std::map<HMIDIOUT, OpenOutput> g_outputs;
    uintptr_t     g_nextHandle  = 0x100;
    juce::uint32  g_startMillis = 0;

    // Deliver one injected MidiMessage to a single (started) input handle from the injecting
    // thread. We never hold g_mutex while calling into JUCE: JUCE's midiInCallback re-enters
    // midiInAddBuffer (via writeFinishedBlocks) which would otherwise deadlock/re-lock.
    void deliverToInput (HMIDIIN h, const juce::MidiMessage& msg)
    {
        MidiInProcT cb = nullptr;
        DWORD_PTR   inst = 0;
        {
            std::lock_guard<std::mutex> lock (g_mutex);
            auto it = g_inputs.find (h);
            if (it == g_inputs.end() || ! it->second.started)
                return;
            cb   = it->second.callback;
            inst = it->second.instance;
        }
        if (cb == nullptr)
            return;

        const DWORD_PTR ts = (DWORD_PTR) (juce::Time::getMillisecondCounter() - g_startMillis);
        const int   size = msg.getRawDataSize();
        const auto* data = msg.getRawData();

        if (msg.isSysEx() || size > 3)
        {
            // Deliver in <=dwBufferLength chunks, each via its own MIM_LONGDATA (JUCE's
            // MidiDataConcatenator reassembles). After each callback JUCE re-adds the finished
            // header (writeFinishedBlocks -> midiInAddBuffer), replenishing the buffer queue.
            int off = 0, remaining = size;
            while (remaining > 0)
            {
                MIDIHDR* hdr = nullptr;
                {
                    std::lock_guard<std::mutex> lock (g_mutex);
                    auto it = g_inputs.find (h);
                    if (it != g_inputs.end() && ! it->second.buffers.empty())
                    {
                        hdr = it->second.buffers.front();
                        it->second.buffers.pop_front();
                    }
                }
                if (hdr == nullptr)
                    return; // no buffer available; drop the remainder

                const int chunk = juce::jmin (remaining, (int) hdr->dwBufferLength);
                std::memcpy (hdr->lpData, data + off, (size_t) chunk);
                hdr->dwBytesRecorded = (DWORD) chunk;
                hdr->dwFlags |= MHDR_DONE;
                off += chunk;
                remaining -= chunk;

                cb (h, MIM_LONGDATA, inst, (DWORD_PTR) hdr, ts);
            }
        }
        else
        {
            const juce::uint8 status = size > 0 ? data[0] : 0;
            const juce::uint8 d1     = size > 1 ? data[1] : 0;
            const juce::uint8 d2     = size > 2 ? data[2] : 0;
            const DWORD_PTR packed = (DWORD_PTR) status
                                   | ((DWORD_PTR) d1 << 8)
                                   | ((DWORD_PTR) d2 << 16);
            cb (h, MIM_DATA, inst, packed, ts);
        }
    }

    // Notifier registered with MockMidi: drains waitingMidiInput and dispatches to every started
    // input (fixture has exactly one). Runs on the injecting (test) thread.
    void dispatchInjectedInput()
    {
        MockMidi* mock = MockMidi::getInstance();
        if (mock == nullptr)
            return;

        for (;;)
        {
            juce::MidiMessage msg;
            {
                std::lock_guard<std::mutex> lock (mock->inputMutex);
                if (mock->waitingMidiInput.empty())
                    break;
                msg = mock->waitingMidiInput.front();
                mock->waitingMidiInput.pop_front();
            }

            std::vector<HMIDIIN> started;
            {
                std::lock_guard<std::mutex> lock (g_mutex);
                for (auto& kv : g_inputs)
                    if (kv.second.started)
                        started.push_back (kv.first);
            }
            for (auto h : started)
                deliverToInput (h, msg);
        }
    }

    // ---- winmm entry points (patched into the IAT) -----------------------------------------
    UINT WINAPI mock_midiInGetNumDevs (void)  { return (UINT) mockNumEndpoints(); }
    UINT WINAPI mock_midiOutGetNumDevs (void) { return (UINT) mockNumEndpoints(); }

    MMRESULT WINAPI mock_midiInGetDevCapsW (UINT_PTR id, LPMIDIINCAPSW p, UINT cbmic)
    {
        if (p == nullptr) return MMSYSERR_INVALPARAM;
        const int np = mockNumPorts();
        if (np <= 0 || (int) id >= mockNumEndpoints()) return MMSYSERR_BADDEVICEID;

        MIDIINCAPSW c = {};
        c.wMid = 1; c.wPid = 1; c.vDriverVersion = 0x0100; c.dwSupport = 0;
        _snwprintf (c.szPname, 31, L"Device %02d, port %02d", (int) id / np, (int) id % np);
        std::memcpy (p, &c, juce::jmin ((size_t) cbmic, sizeof (c)));
        return MMSYSERR_NOERROR;
    }

    MMRESULT WINAPI mock_midiInGetDevCapsA (UINT_PTR id, LPMIDIINCAPSA p, UINT cbmic)
    {
        if (p == nullptr) return MMSYSERR_INVALPARAM;
        const int np = mockNumPorts();
        if (np <= 0 || (int) id >= mockNumEndpoints()) return MMSYSERR_BADDEVICEID;

        MIDIINCAPSA c = {};
        c.wMid = 1; c.wPid = 1; c.vDriverVersion = 0x0100; c.dwSupport = 0;
        _snprintf (c.szPname, 31, "Device %02d, port %02d", (int) id / np, (int) id % np);
        std::memcpy (p, &c, juce::jmin ((size_t) cbmic, sizeof (c)));
        return MMSYSERR_NOERROR;
    }

    MMRESULT WINAPI mock_midiOutGetDevCapsW (UINT_PTR id, LPMIDIOUTCAPSW p, UINT cbmoc)
    {
        if (p == nullptr) return MMSYSERR_INVALPARAM;
        const int np = mockNumPorts();
        if (np <= 0 || (int) id >= mockNumEndpoints()) return MMSYSERR_BADDEVICEID;

        MIDIOUTCAPSW c = {};
        c.wMid = 1; c.wPid = 1; c.vDriverVersion = 0x0100;
        c.wTechnology = MOD_MIDIPORT; // not MOD_MAPPER -> getDefaultDevice() picks index 0
        c.wVoices = 0; c.wNotes = 0; c.wChannelMask = 0xffff; c.dwSupport = 0;
        _snwprintf (c.szPname, 31, L"Device %02d, port %02d", (int) id / np, (int) id % np);
        std::memcpy (p, &c, juce::jmin ((size_t) cbmoc, sizeof (c)));
        return MMSYSERR_NOERROR;
    }

    MMRESULT WINAPI mock_midiOutGetDevCapsA (UINT_PTR id, LPMIDIOUTCAPSA p, UINT cbmoc)
    {
        if (p == nullptr) return MMSYSERR_INVALPARAM;
        const int np = mockNumPorts();
        if (np <= 0 || (int) id >= mockNumEndpoints()) return MMSYSERR_BADDEVICEID;

        MIDIOUTCAPSA c = {};
        c.wMid = 1; c.wPid = 1; c.vDriverVersion = 0x0100;
        c.wTechnology = MOD_MIDIPORT;
        c.wVoices = 0; c.wNotes = 0; c.wChannelMask = 0xffff; c.dwSupport = 0;
        _snprintf (c.szPname, 31, "Device %02d, port %02d", (int) id / np, (int) id % np);
        std::memcpy (p, &c, juce::jmin ((size_t) cbmoc, sizeof (c)));
        return MMSYSERR_NOERROR;
    }

    // DRV_QUERYDEVICEINTERFACESIZE -> NOTSUPPORTED makes JUCE fall back to name-as-identifier.
    MMRESULT WINAPI mock_midiInMessage  (HMIDIIN, UINT, DWORD_PTR, DWORD_PTR)  { return MMSYSERR_NOTSUPPORTED; }
    MMRESULT WINAPI mock_midiOutMessage (HMIDIOUT, UINT, DWORD_PTR, DWORD_PTR) { return MMSYSERR_NOTSUPPORTED; }

    MMRESULT WINAPI mock_midiInOpen (LPHMIDIIN ph, UINT uID, DWORD_PTR cb, DWORD_PTR inst, DWORD)
    {
        const int np = mockNumPorts();
        if (np <= 0 || (int) uID >= mockNumEndpoints()) return MMSYSERR_BADDEVICEID;

        const int dev = (int) uID / np, port = (int) uID % np;
        HMIDIIN h;
        {
            std::lock_guard<std::mutex> lock (g_mutex);
            h = (HMIDIIN) (uintptr_t) (g_nextHandle++);
            OpenInput oi;
            oi.callback = (MidiInProcT) cb;
            oi.instance = inst;
            oi.dev = dev; oi.port = port;
            g_inputs[h] = oi;
        }
        if (ph != nullptr) *ph = h;

        if (MockMidi* m = MockMidi::getInstance())
            m->openInput (dev, port);
        return MMSYSERR_NOERROR;
    }

    MMRESULT WINAPI mock_midiInClose (HMIDIIN h)
    {
        std::lock_guard<std::mutex> lock (g_mutex);
        g_inputs.erase (h);
        return MMSYSERR_NOERROR;
    }

    MMRESULT WINAPI mock_midiInStart (HMIDIIN h)
    {
        std::lock_guard<std::mutex> lock (g_mutex);
        auto it = g_inputs.find (h);
        if (it != g_inputs.end()) it->second.started = true;
        return MMSYSERR_NOERROR;
    }
    MMRESULT WINAPI mock_midiInStop (HMIDIIN h)
    {
        std::lock_guard<std::mutex> lock (g_mutex);
        auto it = g_inputs.find (h);
        if (it != g_inputs.end()) it->second.started = false;
        return MMSYSERR_NOERROR;
    }
    MMRESULT WINAPI mock_midiInReset (HMIDIIN h) { return mock_midiInStop (h); }

    MMRESULT WINAPI mock_midiInPrepareHeader (HMIDIIN, LPMIDIHDR hdr, UINT)
    {
        if (hdr != nullptr) hdr->dwFlags |= MHDR_PREPARED;
        return MMSYSERR_NOERROR;
    }
    MMRESULT WINAPI mock_midiInUnprepareHeader (HMIDIIN, LPMIDIHDR hdr, UINT)
    {
        if (hdr != nullptr) hdr->dwFlags &= ~(DWORD) (MHDR_PREPARED | MHDR_DONE | MHDR_INQUEUE);
        return MMSYSERR_NOERROR;
    }
    MMRESULT WINAPI mock_midiInAddBuffer (HMIDIIN h, LPMIDIHDR hdr, UINT)
    {
        if (hdr == nullptr) return MMSYSERR_INVALPARAM;
        hdr->dwBytesRecorded = 0;
        hdr->dwFlags &= ~(DWORD) MHDR_DONE;
        hdr->dwFlags |= MHDR_INQUEUE;
        std::lock_guard<std::mutex> lock (g_mutex);
        auto it = g_inputs.find (h);
        if (it != g_inputs.end()) it->second.buffers.push_back (hdr);
        return MMSYSERR_NOERROR;
    }

    MMRESULT WINAPI mock_midiOutOpen (LPHMIDIOUT ph, UINT uID, DWORD_PTR, DWORD_PTR, DWORD)
    {
        const int np = mockNumPorts();
        if (np <= 0 || (int) uID >= mockNumEndpoints()) return MMSYSERR_BADDEVICEID;

        const int dev = (int) uID / np, port = (int) uID % np;
        HMIDIOUT h;
        {
            std::lock_guard<std::mutex> lock (g_mutex);
            h = (HMIDIOUT) (uintptr_t) (g_nextHandle++);
            g_outputs[h] = { dev, port };
        }
        if (ph != nullptr) *ph = h;

        if (MockMidi* m = MockMidi::getInstance())
            m->openOutput (dev, port);
        return MMSYSERR_NOERROR;
    }

    MMRESULT WINAPI mock_midiOutClose (HMIDIOUT h)
    {
        std::lock_guard<std::mutex> lock (g_mutex);
        g_outputs.erase (h);
        return MMSYSERR_NOERROR;
    }

    bool lookupOutput (HMIDIOUT h, int& dev, int& port)
    {
        std::lock_guard<std::mutex> lock (g_mutex);
        auto it = g_outputs.find (h);
        if (it == g_outputs.end()) return false;
        dev = it->second.dev; port = it->second.port;
        return true;
    }

    MMRESULT WINAPI mock_midiOutShortMsg (HMIDIOUT h, DWORD dw)
    {
        int dev, port;
        if (! lookupOutput (h, dev, port)) return MMSYSERR_NOERROR;

        // JUCE's MidiOutput background thread can outlive the per-test StrictMock; drop late
        // sends rather than dereferencing a null instance (same hazard the ALSA mock documents).
        MockMidi* m = MockMidi::getInstance();
        if (m == nullptr) return MMSYSERR_NOERROR;

        const juce::uint8 status = (juce::uint8) (dw & 0xFF);
        const juce::uint8 d1     = (juce::uint8) ((dw >> 8) & 0xFF);
        const juce::uint8 d2     = (juce::uint8) ((dw >> 16) & 0xFF);
        const int len = juce::MidiMessage::getMessageLengthFromFirstByte (status);
        juce::MidiMessage msg = len <= 1 ? juce::MidiMessage (status)
                              : len == 2 ? juce::MidiMessage (status, d1)
                                         : juce::MidiMessage (status, d1, d2);
        m->sendMidiEvent (dev, port, msg);
        return MMSYSERR_NOERROR;
    }

    MMRESULT WINAPI mock_midiOutLongMsg (HMIDIOUT h, LPMIDIHDR hdr, UINT)
    {
        if (hdr == nullptr) return MMSYSERR_INVALPARAM;

        int dev, port;
        if (lookupOutput (h, dev, port))
        {
            if (MockMidi* m = MockMidi::getInstance())
            {
                const DWORD n = hdr->dwBufferLength;
                if (hdr->lpData != nullptr && n > 0)
                {
                    juce::MidiMessage msg ((const void*) hdr->lpData, (int) n);
                    m->sendMidiEvent (dev, port, msg);
                }
            }
        }
        // JUCE spins on (dwFlags & MHDR_DONE); set it before returning.
        hdr->dwFlags |= MHDR_DONE;
        return MMSYSERR_NOERROR;
    }

    MMRESULT WINAPI mock_midiOutPrepareHeader (HMIDIOUT, LPMIDIHDR hdr, UINT)
    {
        if (hdr != nullptr) hdr->dwFlags |= MHDR_PREPARED;
        return MMSYSERR_NOERROR;
    }
    MMRESULT WINAPI mock_midiOutUnprepareHeader (HMIDIOUT, LPMIDIHDR hdr, UINT)
    {
        if (hdr != nullptr) hdr->dwFlags &= ~(DWORD) (MHDR_PREPARED | MHDR_DONE | MHDR_INQUEUE);
        return MMSYSERR_NOERROR;
    }

    // ---- IAT patcher -----------------------------------------------------------------------
    struct Replacement { const char* name; void* fn; };

    const Replacement g_replacements[] = {
        { "midiInGetNumDevs",       (void*) &mock_midiInGetNumDevs },
        { "midiInGetDevCaps",       (void*) &mock_midiInGetDevCapsW },
        { "midiInGetDevCapsW",      (void*) &mock_midiInGetDevCapsW },
        { "midiInGetDevCapsA",      (void*) &mock_midiInGetDevCapsA },
        { "midiInOpen",             (void*) &mock_midiInOpen },
        { "midiInClose",            (void*) &mock_midiInClose },
        { "midiInStart",            (void*) &mock_midiInStart },
        { "midiInStop",             (void*) &mock_midiInStop },
        { "midiInReset",            (void*) &mock_midiInReset },
        { "midiInPrepareHeader",    (void*) &mock_midiInPrepareHeader },
        { "midiInUnprepareHeader",  (void*) &mock_midiInUnprepareHeader },
        { "midiInAddBuffer",        (void*) &mock_midiInAddBuffer },
        { "midiInMessage",          (void*) &mock_midiInMessage },
        { "midiOutGetNumDevs",      (void*) &mock_midiOutGetNumDevs },
        { "midiOutGetDevCaps",      (void*) &mock_midiOutGetDevCapsW },
        { "midiOutGetDevCapsW",     (void*) &mock_midiOutGetDevCapsW },
        { "midiOutGetDevCapsA",     (void*) &mock_midiOutGetDevCapsA },
        { "midiOutOpen",            (void*) &mock_midiOutOpen },
        { "midiOutClose",           (void*) &mock_midiOutClose },
        { "midiOutShortMsg",        (void*) &mock_midiOutShortMsg },
        { "midiOutLongMsg",         (void*) &mock_midiOutLongMsg },
        { "midiOutPrepareHeader",   (void*) &mock_midiOutPrepareHeader },
        { "midiOutUnprepareHeader", (void*) &mock_midiOutUnprepareHeader },
        { "midiOutMessage",         (void*) &mock_midiOutMessage },
    };

    void* lookupReplacement (const char* name)
    {
        for (const auto& r : g_replacements)
            if (std::strcmp (r.name, name) == 0)
                return r.fn;
        return nullptr;
    }

    void patchWinmmImports()
    {
        auto* base = reinterpret_cast<BYTE*> (GetModuleHandleW (nullptr));
        if (base == nullptr) return;

        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*> (base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;

        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*> (base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return;

        const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (dir.VirtualAddress == 0) return;

        auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*> (base + dir.VirtualAddress);
        for (; imp->Name != 0; ++imp)
        {
            const char* dllName = reinterpret_cast<const char*> (base + imp->Name);
            if (_stricmp (dllName, "winmm.dll") != 0)
                continue;

            auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*> (base + imp->FirstThunk);
            auto* intt = imp->OriginalFirstThunk != 0
                            ? reinterpret_cast<IMAGE_THUNK_DATA*> (base + imp->OriginalFirstThunk)
                            : iat;

            for (; intt->u1.AddressOfData != 0; ++intt, ++iat)
            {
                if (IMAGE_SNAP_BY_ORDINAL (intt->u1.Ordinal))
                    continue; // imported by ordinal; the JUCE midi imports are all by name

                auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*> (base + intt->u1.AddressOfData);
                void* repl = lookupReplacement (reinterpret_cast<const char*> (ibn->Name));
                if (repl == nullptr)
                    continue;

                DWORD oldProtect = 0;
                if (VirtualProtect (&iat->u1.Function, sizeof (iat->u1.Function), PAGE_READWRITE, &oldProtect))
                {
                    iat->u1.Function = (ULONGLONG) (uintptr_t) repl;
                    VirtualProtect (&iat->u1.Function, sizeof (iat->u1.Function), oldProtect, &oldProtect);
                }
            }
        }
    }

    // Patch the IAT and register the input notifier once, at static-init time -- safely before
    // JUCE's lazy MidiService singleton is created (first getAvailableDevices() call in a test).
    struct RegisterWin32Mock
    {
        RegisterWin32Mock()
        {
            g_startMillis = juce::Time::getMillisecondCounter();
            MockMidi::setSubsystemInputNotifier (dispatchInjectedInput);
            patchWinmmImports();
        }
    } registerWin32Mock;
} // namespace

#endif // JUCE_WINDOWS
