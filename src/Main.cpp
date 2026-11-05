#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/MainWindow.h"

class LiveKeysApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "LiveKeys"; }
    const juce::String getApplicationVersion() override    { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (LiveKeysApp)
