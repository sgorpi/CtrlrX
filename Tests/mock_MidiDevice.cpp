#include "mock_MidiDevice.h"

MockMidi* MockMidi::latestInstance = nullptr;
bool MockMidi::hasSubsystemMockRegistered = false;
std::function<void()> MockMidi::subsystemInputNotifier = nullptr;