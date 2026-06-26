#include "test_ProcessorFixture.h"

#include "CtrlrManager.h"
#include "CtrlrPanel.h"
#include "CtrlrModulator.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

/**
 * NRPN (latch & stream) and SysEx tests.
 *
 * These pin down the deterministic invariants of the multi-message (NRPN) and SysEx paths:
 *  - NRPN latch/stream emits the header CCs (99/98) ONCE and then streams data only (CC 6/38),
 *    in the correct order (the recent fef3b8d8 feature) -- characterization,
 *  - a SysEx-token modulator emits its message exactly once on a value change, and a matching
 *    SysEx injected from the device updates the modulator -- characterization.
 *
 * Device output is delivered asynchronously by JUCE's MidiOutput background thread, so the tests
 * record sends from the StrictMock under a lock and wait briefly for delivery before asserting.
 * Seam: black-box -- setParameter / processBlock / injectMidiInput in, StrictMock device out.
 */

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;

// Shared helpers (allowAndRecordDeviceSends / loadPanel / takeSendsAfterDelivery /
// controllerNumbers) now live in test_ProcessorFixture.h.
class MidiSysex : public ProcessorInstance
{
};

/**
 * NRPN latch & stream: the first value change emits the NRPN header (CC 99/98) followed by the data
 * entry CCs (6/38), in order. A second value change (same parameter) streams the data CCs only --
 * the header is latched and emitted just once. Characterization of fef3b8d8.
 */
TEST_F(MidiSysex, nrpn_latch_stream_emits_header_once_then_data_only)
{
    allowAndRecordDeviceSends();
    ASSERT_NE(loadPanel("fixture_nrpn_stream.panel"), nullptr);
    processor->prepareToPlay(44100, BLOCK_SIZE);

    // First change: full NRPN (header + data).
    processor->setParameter(1, 0.4);
    processor->processBlock(buffer, midiMessages);
    auto first = controllerNumbers(takeSendsAfterDelivery());
    EXPECT_EQ(first, (std::vector<int>{99, 98, 6, 38}))
        << "first NRPN send should be header CC99/CC98 then data CC6/CC38, in order";

    // Second change: data only -- the header is latched.
    processor->setParameter(1, 0.8);
    processor->processBlock(buffer, midiMessages);
    auto second = controllerNumbers(takeSendsAfterDelivery());
    EXPECT_EQ(second, (std::vector<int>{6, 38}))
        << "second NRPN send should stream data only (header latched, emitted once)";
}

/**
 * A SysEx-token modulator emits its SysEx message exactly once per value change, with the value
 * byte filled in at the token position ("F0 7D xx F7"). Characterization.
 */
TEST_F(MidiSysex, sysex_token_emitted_once_on_value_change)
{
    allowAndRecordDeviceSends();
    ASSERT_NE(loadPanel("fixture_sysex_token.panel"), nullptr);
    processor->prepareToPlay(44100, BLOCK_SIZE);

    processor->setParameter(1, 0.5); // 0.5 * 127 -> 64 (0x40)
    processor->processBlock(buffer, midiMessages);
    auto sends = takeSendsAfterDelivery();

    int sysexCount = 0;
    juce::MidiMessage sysex;
    for (const auto& m : sends)
        if (m.isSysEx()) { sysexCount++; sysex = m; }

    ASSERT_EQ(sysexCount, 1) << "exactly one SysEx message should be emitted per value change";

    // Expect F0 7D 40 F7 (value byte at the 'xx' position).
    ASSERT_EQ(sysex.getRawDataSize(), 4);
    const juce::uint8* d = sysex.getRawData();
    EXPECT_EQ(d[0], 0xF0);
    EXPECT_EQ(d[1], 0x7D);
    EXPECT_EQ(d[2], 64);
    EXPECT_EQ(d[3], 0xF7);
}

/**
 * A matching SysEx injected from the device updates the modulator (the SysEx input comparator
 * extracts the value byte). Characterization of the SysEx input-compare path.
 */
TEST_F(MidiSysex, sysex_input_updates_modulator)
{
    if (!midi_mock.hasSubsystemMock())
        GTEST_SKIP() << "no MIDI subsystem mock on this platform; device input cannot be injected";

    allowAndRecordDeviceSends();
    ASSERT_NE(loadPanel("fixture_sysex_token.panel"), nullptr);
    processor->prepareToPlay(44100, BLOCK_SIZE);

    // Matching SysEx with value byte 100 at the 'xx' position: F0 7D 64 F7.
    const juce::uint8 raw[] = { 0xF0, 0x7D, 100, 0xF7 };
    const juce::MidiMessage sysex(raw, (int) sizeof(raw));
    MockMidi::getInstance()->injectMidiInput(sysex);

    for (int i = 0; i < 24; i++)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
    for (int i = 0; i < 4; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(23));
        processor->processBlock(buffer, midiMessages);
        midiMessages.clear();
    }

    EXPECT_FLOAT_EQ(processor->getParameter(1), 100.0f / 127.0f)
        << "modulator parameter should track the value carried by the matched input SysEx";
}
