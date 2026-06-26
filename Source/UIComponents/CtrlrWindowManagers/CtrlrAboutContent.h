#pragma once

#include "CtrlrChildWindowContent.h"
#include "../CtrlrAbout.h"
#include "CtrlrManagerWindowManager.h"

class CtrlrAboutContent : public CtrlrChildWindowContent
{
public:
    CtrlrAboutContent(CtrlrManager &owner)
    {
        about = new CtrlrAbout(owner);
        addAndMakeVisible(about);
        setSize(about->getWidth(), about->getHeight());
    }

    ~CtrlrAboutContent()
    {
        deleteAndZero(about);
    }

    String getContentName() { return (String("About")); }
    uint8 getType() { return (CtrlrManagerWindowManager::AboutWindow); }
    ValueTree &saveState() { return (componentTree); }

    void resized() override
    {
        if (about)
            about->setBounds(getLocalBounds());
    }

private:
    CtrlrAbout *about;
};
