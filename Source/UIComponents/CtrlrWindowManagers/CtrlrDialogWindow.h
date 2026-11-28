#pragma once
#include "JuceHeader.h"

class CtrlrDialogWindow
{
public:
    // -------------------------
    // Modal dialogs (all platforms)
    // -------------------------
    static int showModalDialog(const juce::String& title,
        juce::Component* content,
        const bool resizable = false,
        juce::Component* parent = nullptr);

    // The same window without a modal loop. The window owns the content; the callback gets the
    // result and the content when the window has closed.
    static void showModalDialogAsync(const juce::String& title,
        std::unique_ptr<juce::Component> content,
        const bool resizable,
        juce::Component* parent,
        std::function<void (int result, juce::Component& content)> callback);

    // -------------------------
    // Non-modal dialogs (all platforms)
    // Opens completely offscreen first on Linux to prevent flicker
    // Caller owns the returned pointer
    // -------------------------
    static juce::DialogWindow* showNonModalDialog(const juce::String& title,
        juce::Component* content,
        const juce::Colour& backgroundColour = juce::Colours::lightgrey,
        bool escapeKeyCloses = true,
        bool resizable = false);

private:
    static void runAsync(std::unique_ptr<juce::DialogWindow> window,
        std::unique_ptr<juce::Component> content,
        std::function<void (int, juce::Component&)> callback);

    JUCE_DECLARE_NON_COPYABLE(CtrlrDialogWindow)
};
