#pragma once

#include "CtrlrChildWindowContent.h"
#include "../CtrlrApplicationWindow/CtrlrSettings.h"
#include "CtrlrManagerWindowManager.h"

class CtrlrSettingsContent : public CtrlrChildWindowContent
{
public:
    CtrlrSettingsContent(CtrlrManager &owner)
    {
        settings = new CtrlrSettings(owner);
        addAndMakeVisible(settings);
        setSize(settings->getWidth(), settings->getHeight());
        container = nullptr;
    }

    ~CtrlrSettingsContent()
    {
        deleteAndZero(settings);
    }

    String getContentName() { return (String("Global Settings")); }
    uint8 getType() { return (CtrlrManagerWindowManager::GlobalSettings); }
    ValueTree &saveState() { return (componentTree); }

    void resized() override
    {
        if (settings)
            settings->setBounds(getLocalBounds());
    }

private:
    CtrlrSettings *settings;
};
