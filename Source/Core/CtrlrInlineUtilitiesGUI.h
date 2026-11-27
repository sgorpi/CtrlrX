#ifndef CTRLR_INLINE_UTILITIES_GUI
#define CTRLR_INLINE_UTILITIES_GUI

#include "CtrlrMacros.h"
#include <juce_gui_basics/juce_gui_basics.h> // Make sure this is included for LookAndFeel_V4
#include <deque>

namespace gui {

    static inline
    DrawableButton *createDrawableButton(const String &buttonName, const String &svgData,
										 const String &svgDataDown="",
										 const MouseCursor cursor = MouseCursor::PointingHandCursor) {
        std::unique_ptr<XmlElement> svgXml (XmlDocument::parse(svgData));
        std::unique_ptr<Drawable> drawable (Drawable::createFromSVG(*svgXml));

        std::unique_ptr<XmlElement> svgXmlDown (XmlDocument::parse(svgDataDown));
        std::unique_ptr<Drawable> drawableDown (svgXmlDown ? Drawable::createFromSVG(*svgXmlDown) : nullptr);

        auto btn = new DrawableButton(buttonName, DrawableButton::ImageFitted);
        btn->setImages(drawable.get(),
                       nullptr,
                       drawableDown.get(),
                       nullptr,
                       nullptr,
                       nullptr,
                       nullptr,
                       nullptr);
        btn->setMouseCursor(cursor);
        return btn;
    }

    static inline
    Drawable *createDrawable(const String &svgData) {
        std::unique_ptr<XmlElement> svgXml (XmlDocument::parse(svgData));
        return Drawable::createFromSVG(*svgXml).release();
    }

    static const inline void
    drawSelectionRectangle(Graphics &g, int width, int height, Colour base = Colour(HIGHLIGHT_COLOUR),
                           const float baseSaturation = 0.9f, const float baseAlpha = 0.9f,
                           const float gradientMin = 0.2f, const float gradientMax = 0.25f) {
        Colour baseColour(base.withMultipliedSaturation(baseSaturation).withMultipliedAlpha(baseAlpha));
        const float mainBrightness = baseColour.getBrightness();
        const float mainAlpha = baseColour.getFloatAlpha();
        Path outline;
        outline.addRoundedRectangle(0, 0, width, height, 4.0f, 4.0f, false, false, false, false);
        g.setGradientFill(ColourGradient(baseColour.brighter(gradientMin), 0.0f, 0.0f,
                                         baseColour.darker(gradientMax), 0.0f, height, false));
        g.fillPath(outline);

        g.setColour(Colours::white.withAlpha(0.4f * mainAlpha * mainBrightness * mainBrightness));
        g.strokePath(outline, PathStrokeType(1.0f),
                     AffineTransform::translation(0.0f, 1.0f).scaled(1.0f, (height - 1.6f) / height));
        g.setColour(Colours::black.withAlpha(0.4f * mainAlpha));
        g.strokePath(outline, PathStrokeType(1.0f));
    }

    static const inline void
    drawSelectionRectangle(Graphics &g, int x, int y, int width, int height, Colour base = Colour(HIGHLIGHT_COLOUR),
                           const float baseSaturation = 0.9f, const float baseAlpha = 0.9f) {
        Colour baseColour(base.withMultipliedSaturation(baseSaturation).withMultipliedAlpha(baseAlpha));
        const float mainBrightness = baseColour.getBrightness();
        const float mainAlpha = baseColour.getFloatAlpha();
        Path outline;

        outline.addRoundedRectangle(x, y, width, height, 4.0f, 4.0f, false, false, false, false);

        g.setGradientFill(ColourGradient(baseColour.brighter(0.2f), x, y, baseColour.darker(0.25f), x, height, false));
        g.fillPath(outline);

        g.setColour(Colours::white.withAlpha(0.4f * mainAlpha * mainBrightness * mainBrightness));
        g.strokePath(outline, PathStrokeType(1.0f),
                     AffineTransform::translation(0.0f, 1.0f).scaled(1.0f, (height - 1.6f) / height));
        g.setColour(Colours::black.withAlpha(0.4f * mainAlpha));
        g.strokePath(outline, PathStrokeType(1.0f));
    }

    // static LookAndFeel_V4::ColourScheme colourSchemeFromProperty(const var &property) {} // Updated v5.6.34. Moved to CtrlrInlineUtilitiesGUI.cpp

    // Declare your custom ColourScheme getter functions
    juce::LookAndFeel_V4::ColourScheme getJetBlackColourScheme();
    juce::LookAndFeel_V4::ColourScheme getYamDxColourScheme();
    juce::LookAndFeel_V4::ColourScheme getAkApcColourScheme();
    juce::LookAndFeel_V4::ColourScheme getAkMpcColourScheme();
    juce::LookAndFeel_V4::ColourScheme getLexiBlueColourScheme();
    juce::LookAndFeel_V4::ColourScheme getKurzGreenColourScheme();
    juce::LookAndFeel_V4::ColourScheme getKorGreyColourScheme();
    juce::LookAndFeel_V4::ColourScheme getKorGoldColourScheme();
    juce::LookAndFeel_V4::ColourScheme getArturOrangeColourScheme();
    juce::LookAndFeel_V4::ColourScheme getAiraGreenColourScheme();

    // Your existing colourSchemeFromProperty function
    juce::LookAndFeel_V4::ColourScheme colourSchemeFromProperty(const juce::var &property);

    // The central LookAndFeel factory function
    // Keep the optional 'colourSchemeProperty' parameter, it's not directly used here but the signature is fine.
    juce::LookAndFeel* createLookAndFeelFromDescription(const juce::String& description,
                                                        const juce::var& colourSchemeProperty = juce::var(),
                                                        bool returnDefaultV4ForUnknown = true);

    // Your existing colourSchemeFromProperty function (can now call createLookAndFeelFromDescription if it uses properties
    // that map directly to the names used in createLookAndFeelFromDescription)
    juce::LookAndFeel_V4::ColourScheme colourSchemeFromProperty(const juce::var &property);

    // Added v8.0.85. JUCE 8's LookAndFeel::findColour() asserts on a colour ID that the LookAndFeel
    // does not define. JUCE 6 in 5.6.37 had that jassert commented out (v5.6.34) and returned black.
    // Define these IDs as black where they are missing, so the colours stay the same and Debug
    // builds don't stop on panel load. From John Goodland's v8.08/v8.18 (dnaldoog e2ad82542).
    inline void initLookAndFeelDefaults(juce::LookAndFeel &lf)
    {
        const int ids[] = {juce::Label::backgroundWhenEditingColourId,
                           juce::Label::textWhenEditingColourId,
                           juce::Label::outlineWhenEditingColourId,
                           juce::ScrollBar::backgroundColourId,
                           juce::ScrollBar::thumbColourId,
                           juce::ScrollBar::trackColourId,
                           juce::TextEditor::highlightColourId,
                           juce::TextEditor::highlightedTextColourId,
                           juce::TextEditor::outlineColourId,
                           juce::TextButton::textColourOffId,
                           juce::ProgressBar::foregroundColourId,
                           juce::CodeEditorComponent::backgroundColourId,
                           juce::CodeEditorComponent::defaultTextColourId};

        for (const int id : ids)
            if (!lf.isColourSpecified(id))
                lf.setColour(id, juce::Colours::black);
    }
}
// Added v8.0.100. Asynchronous file choosers, after John Goodland's FC helpers (his v8.49 to v8.79).
// FileChooser::launchAsync() returns at once and calls the callback once the user has answered,
// with an empty File (or array) for Cancel. The flags are those of the blocking browseFor...()
// calls they replace; saveFileAsync() keeps the overwrite question of browseForFileToSave (true).
// Each helper keeps its FileChooser alive in a std::shared_ptr that the callback holds; JUCE drops
// the callback when the chooser closes, which ends that cycle. A callback that uses a panel or a
// component must check that it still exists: the user can close it while the chooser is open.
namespace FC
{
    // Test hook. When set, no chooser is shown: the hook gets the title and the
    // FileBrowserComponent flags and returns the answer (an empty array for Cancel). The callback
    // still runs later, from the message loop, as it does after a real chooser.
    using TestAnswer = std::function<juce::Array<juce::File> (const juce::String &title, int flags)>;

    inline TestAnswer &testAnswer()
    {
        static TestAnswer hook;
        return hook;
    }

    inline void launchAsync (const juce::String &dialogTitle, const juce::File &initialFileOrDirectory,
                             const juce::String &filePatternsAllowed, bool useNativeDialog, int flags,
                             std::function<void (const juce::Array<juce::File> &)> callback)
    {
        if (const TestAnswer &hook = testAnswer())
        {
            const juce::Array<juce::File> answer = hook (dialogTitle, flags);
            juce::MessageManager::callAsync ([answer, callback] {
                if (callback)
                    callback (answer);
            });
            return;
        }

        auto chooser = std::make_shared<juce::FileChooser> (dialogTitle, initialFileOrDirectory,
                                                            filePatternsAllowed, useNativeDialog);
        // The callback runs one message later, once the chooser window is gone: the code after a
        // blocking chooser ran after it had closed, and it may show an alert.
        chooser->launchAsync (flags, [chooser, callback] (const juce::FileChooser &fc) {
            const juce::Array<juce::File> results = fc.getResults();
            juce::MessageManager::callAsync ([results, callback] {
                if (callback)
                    callback (results);
            });
        });
    }

    inline std::function<void (const juce::Array<juce::File> &)> firstOf (std::function<void (const juce::File &)> callback)
    {
        return [callback] (const juce::Array<juce::File> &files) {
            if (callback)
                callback (files.isEmpty() ? juce::File() : files.getFirst());
        };
    }

    inline void saveFileAsync (const juce::String &dialogTitle, const juce::File &initialFileOrDirectory,
                               const juce::String &filePatternsAllowed, bool useNativeDialog,
                               std::function<void (const juce::File &)> callback)
    {
        launchAsync (dialogTitle, initialFileOrDirectory, filePatternsAllowed, useNativeDialog,
                     juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting,
                     firstOf (std::move (callback)));
    }

    inline void openFileAsync (const juce::String &dialogTitle, const juce::File &initialFileOrDirectory,
                               const juce::String &filePatternsAllowed, bool useNativeDialog,
                               std::function<void (const juce::File &)> callback)
    {
        launchAsync (dialogTitle, initialFileOrDirectory, filePatternsAllowed, useNativeDialog,
                     juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                     firstOf (std::move (callback)));
    }

    inline void openMultipleFilesAsync (const juce::String &dialogTitle, const juce::File &initialFileOrDirectory,
                                        const juce::String &filePatternsAllowed, bool useNativeDialog,
                                        std::function<void (const juce::Array<juce::File> &)> callback)
    {
        launchAsync (dialogTitle, initialFileOrDirectory, filePatternsAllowed, useNativeDialog,
                     juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::canSelectMultipleItems,
                     std::move (callback));
    }

    // For browseForDirectory(). The patterns are passed on as the blocking chooser did.
    inline void chooseDirectoryAsync (const juce::String &dialogTitle, const juce::File &initialFileOrDirectory,
                                      const juce::String &filePatternsAllowed, bool useNativeDialog,
                                      std::function<void (const juce::File &)> callback)
    {
        launchAsync (dialogTitle, initialFileOrDirectory, filePatternsAllowed, useNativeDialog,
                     juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                     firstOf (std::move (callback)));
    }
}

// Added v8.0.100. Asynchronous popup menus, after John Goodland's PU helpers (his v8.04 to v8.79).
// PopupMenu::showMenuAsync() returns at once and calls the callback with the chosen item ID, or 0
// when the menu was dismissed. show() and showAt() pass the options of the blocking
// PopupMenu::show() and showAt() they replace, so the menu opens where it did. A callback that
// uses a component must check that it still exists: Lua or the host can delete it while the menu
// is open.
namespace PU
{
    // Test hook. When set, no menu is shown: the hook gets the menu and its options and returns
    // the item ID to choose (0 = dismissed). The callback still runs later, from the message loop,
    // as it does after a real menu.
    using TestAnswer = std::function<int (const juce::PopupMenu &menu, const juce::PopupMenu::Options &options)>;

    inline TestAnswer &testAnswer()
    {
        static TestAnswer hook;
        return hook;
    }

    inline void showMenuAsync (const juce::PopupMenu &menu, const juce::PopupMenu::Options &options,
                               std::function<void (int)> callback)
    {
        // JUCE opens no window for a menu without items and never calls back; the blocking show()
        // returned 0, and the code after it ran.
        if (! juce::PopupMenu::MenuItemIterator (menu).next())
        {
            juce::MessageManager::callAsync ([callback] {
                if (callback)
                    callback (0);
            });
            return;
        }

        if (const TestAnswer &hook = testAnswer())
        {
            const int answer = hook (menu, options);
            juce::MessageManager::callAsync ([answer, callback] {
                if (callback)
                    callback (answer);
            });
            return;
        }

        // The menu window takes a copy of the items, so the caller's menu may go out of scope.
        juce::PopupMenu shown (menu);
        // The callback runs one message later, once the menu window is gone: JUCE calls it while
        // the window still exists, and the code after a blocking show() ran after it had closed
        // (it may open an alert, a file chooser or another menu).
        shown.showMenuAsync (options, [callback] (int result) {
            juce::MessageManager::callAsync ([result, callback] {
                if (callback)
                    callback (result);
            });
        });
    }

    // For PopupMenu::show(): at the mouse position.
    inline void show (const juce::PopupMenu &menu, std::function<void (int)> callback)
    {
        showMenuAsync (menu, juce::PopupMenu::Options(), std::move (callback));
    }

    // For PopupMenu::showAt (component).
    inline void showAt (const juce::PopupMenu &menu, juce::Component *target, std::function<void (int)> callback)
    {
        juce::PopupMenu::Options options;
        if (target != nullptr)
            options = options.withTargetComponent (target);
        showMenuAsync (menu, options, std::move (callback));
    }
}

// Added v8.0.100. Asynchronous alerts and dialog windows, after John Goodland's AW helpers (his
// v8.04 to v8.79). Each alert helper shows the alert of the blocking AlertWindow::show...Box() it
// replaces (JUCE builds the same window for both forms, or the same native box when the native
// alerts setting is on) and gives the callback the result that call returned. A callback that
// uses a panel or a component must check that it still exists: the user can close it while the
// alert is open.
namespace AW
{
    // What the test hook is asked to answer.
    struct Request
    {
        juce::String title;
        juce::String message;
        juce::StringArray buttons;         // an alert's buttons, in order
        juce::Component *window = nullptr; // a dialog window shown with runModalAsync()
    };

    // Test hook. When set, no alert or dialog window is shown: the hook returns the result the
    // user's answer gives (resultForButton() for an alert; a dialog window's result, after the
    // hook has filled its fields). The callback still runs later, from the message loop.
    using TestAnswer = std::function<int (const Request &request)>;

    inline TestAnswer &testAnswer()
    {
        static TestAnswer hook;
        return hook;
    }

    // The result of an alert's button `index` (0 = the first) as the blocking calls give it: 1 and 0
    // for OK/Cancel, 1, 2 and 0 for Yes/No/Cancel. Escape gives 0.
    inline int resultForButton (int index, int numButtons)
    {
        return numButtons <= 1 ? 0 : (index + 1) % numButtons;
    }

    // The callback runs one message later, once the window is gone: JUCE calls it while the window
    // still exists, and the code after a blocking alert ran after it had closed (it may show
    // another alert or a file chooser).
    inline void callBackLater (std::function<void (int)> callback, int result)
    {
        juce::MessageManager::callAsync ([callback, result] {
            if (callback)
                callback (result);
        });
    }

    // One alert at a time, in the order they were asked for. A blocking alert stopped the code
    // that asked for it, so the alerts of one action (an error, then the summary of that action)
    // came one after the other. Without this queue they would all open at once, the last one on
    // top. The next alert opens after the callback of the one before it has run.
    struct AlertQueue
    {
        // `show` opens its alert and calls `closed` once with the result.
        using Show = std::function<void (std::function<void (int)> closed)>;

        std::deque<std::pair<Show, std::function<void (int)>>> waiting;
        bool open = false;

        static AlertQueue &get()
        {
            static AlertQueue queue;
            return queue;
        }

        void add (Show show, std::function<void (int)> callback)
        {
            waiting.emplace_back (std::move (show), std::move (callback));
            if (! open)
                showNext();
        }

        void showNext()
        {
            if (waiting.empty())
            {
                open = false;
                return;
            }
            open = true;
            auto next = std::move (waiting.front());
            waiting.pop_front();
            auto callback = next.second;
            next.first ([callback] (int result) {
                juce::MessageManager::callAsync ([callback, result] {
                    if (callback)
                        callback (result);
                    AlertQueue::get().showNext();
                });
            });
        }
    };

    // Queues the alert of `options`; `showAlert` opens it with JUCE (when no test hook is set).
    inline void queueAlert (const juce::MessageBoxOptions &options,
                            std::function<void (juce::ModalComponentManager::Callback *)> showAlert,
                            std::function<void (int)> callback)
    {
        AlertQueue::get().add ([options, showAlert] (std::function<void (int)> closed) {
            if (const TestAnswer &hook = testAnswer())
            {
                Request request;
                request.title = options.getTitle();
                request.message = options.getMessage();
                for (int i = 0; i < options.getNumButtons(); ++i)
                    request.buttons.add (options.getButtonText (i));
                closed (hook (request));
                return;
            }
            showAlert (juce::ModalCallbackFunction::create (closed));
        }, std::move (callback));
    }

    // For AlertWindow::showMessageBox().
    inline void showMessageBox (juce::MessageBoxIconType icon, const juce::String &title, const juce::String &message,
                                const juce::String &buttonText = juce::String(),
                                juce::Component *associatedComponent = nullptr,
                                std::function<void()> callback = nullptr)
    {
        // The associated component is held weakly, as JUCE's own options do.
        juce::Component::SafePointer<juce::Component> component (associatedComponent);
        queueAlert (juce::MessageBoxOptions::makeOptionsOk (icon, title, message, buttonText, associatedComponent),
                    [=] (juce::ModalComponentManager::Callback *closed) {
                        juce::AlertWindow::showMessageBoxAsync (icon, title, message, buttonText, component.getComponent(), closed);
                    },
                    [callback] (int) {
                        if (callback)
                            callback();
                    });
    }

    // For AlertWindow::showOkCancelBox(): true for button 1.
    inline void showOkCancelBox (juce::MessageBoxIconType icon, const juce::String &title, const juce::String &message,
                                 const juce::String &button1Text, const juce::String &button2Text,
                                 juce::Component *associatedComponent, std::function<void (bool)> callback)
    {
        juce::Component::SafePointer<juce::Component> component (associatedComponent);
        queueAlert (juce::MessageBoxOptions::makeOptionsOkCancel (icon, title, message, button1Text, button2Text, associatedComponent),
                    [=] (juce::ModalComponentManager::Callback *closed) {
                        // This overload, not AlertWindow::showAsync(): with native alerts,
                        // showAsync() gives the button index, the blocking call the mapped result.
                        juce::AlertWindow::showOkCancelBox (icon, title, message, button1Text, button2Text, component.getComponent(), closed);
                    },
                    [callback] (int result) {
                        if (callback)
                            callback (result == 1);
                    });
    }

    // For AlertWindow::showYesNoCancelBox(): 1 for button 1, 2 for button 2, 0 for button 3.
    inline void showYesNoCancelBox (juce::MessageBoxIconType icon, const juce::String &title, const juce::String &message,
                                    const juce::String &button1Text, const juce::String &button2Text,
                                    const juce::String &button3Text, juce::Component *associatedComponent,
                                    std::function<void (int)> callback)
    {
        juce::Component::SafePointer<juce::Component> component (associatedComponent);
        queueAlert (juce::MessageBoxOptions::makeOptionsYesNoCancel (icon, title, message, button1Text, button2Text,
                                                                    button3Text, associatedComponent),
                    [=] (juce::ModalComponentManager::Callback *closed) {
                        juce::AlertWindow::showYesNoCancelBox (icon, title, message, button1Text, button2Text, button3Text,
                                                               component.getComponent(), closed);
                    },
                    std::move (callback));
    }

    // For runModalLoop() on a window the caller made (an AlertWindow with text boxes, a
    // DialogWindow): shows it modal, as runModalLoop() did. When it closes it is hidden, and one
    // message later the callback gets its result and the window, to read its fields; the window
    // is deleted after the callback.
    template <typename WindowType>
    inline void runModalAsync (std::unique_ptr<WindowType> window, std::function<void (int, WindowType &)> callback)
    {
        std::shared_ptr<WindowType> held (std::move (window));
        const auto finish = [held, callback] (int result) {
            held->setVisible (false);
            juce::MessageManager::callAsync ([held, callback, result] {
                if (callback)
                    callback (result, *held);
            });
        };

        if (const TestAnswer &hook = testAnswer())
        {
            Request request;
            request.title = held->getName();
            request.window = held.get();
            finish (hook (request));
            return;
        }

        held->enterModalState (true, juce::ModalCallbackFunction::create (finish), false);
    }
}

#endif
