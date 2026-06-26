/*
  ==============================================================================

    CtrlrPanelEditorIcons.cpp
    Created: 23 Aug 2025 9:18:11pm
    Author:  zan64

  ==============================================================================
*/

#include "CtrlrPanelEditorIcons.h"

// Define all your SVG strings using juce::String literals
const juce::String SvgIconManager::dragDropIcon = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 640 640">
<path d="M470.6 566.6L566.6 470.6C575.8 461.4 578.5 447.7 573.5 435.7C568.5 423.7 556.9 416 544 416L480 416L480 96C480 78.3 465.7 64 448 64C430.3 64 416 78.3 416 96L416 416L352 416C339.1 416 327.4 423.8 322.4 435.8C317.4 447.8 320.2 461.5 329.3 470.7L425.3 566.7C437.8 579.2 458.1 579.2 470.6 566.7zM214.6 73.4C202.1 60.9 181.8 60.9 169.3 73.4L73.3 169.4C64.1 178.6 61.4 192.3 66.4 204.3C71.4 216.3 83.1 224 96 224L160 224L160 544C160 561.7 174.3 576 192 576C209.7 576 224 561.7 224 544L224 224L288 224C300.9 224 312.6 216.2 317.6 204.2C322.6 192.2 319.8 178.5 310.7 169.3L214.7 73.3z"/></svg>
)";

const juce::String SvgIconManager::dragDropIconThin = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
<path fill="#000000" d="M409,103.2c0,2.5,0,4,0,5.5c0,114,0,227.9,0,341.9c0,14.1-11.5,25.6-25.6,25.6c-14.1,0-25.6-11.6-25.6-25.7c0-113.6,0-227.2,0-340.7c0-1.8,0-3.6,0-6.3c-1.5,1.4-2.5,2.2-3.4,3.1c-18.3,18.3-36.5,36.6-54.8,54.8c-7.2,7.2-16,9.5-25.7,6.7c-9.7-2.8-15.6-9.7-17.8-19.3c-2.3-9.9,1.7-18,8.6-24.9c28.2-28.2,56.5-56.4,84.7-84.7c5.1-5.1,10.2-10,15-15.3c10.1-11.2,28.2-10.3,38.6,0.3c32.5,33.3,65.7,66,98.5,99c1.8,1.8,3.6,3.7,5,5.8c7.8,10.9,6,25.4-4.2,34.2c-9.9,8.5-25.2,7.9-34.6-1.4c-18.4-18.3-36.7-36.7-55.1-55.1C411.7,105.7,410.7,104.8,409,103.2z"/>
<path fill="#000000" d="M102,388.7c0-2,0-3.2,0-4.4c0-114.1,0-228.2,0-342.3c0-14.7,11.4-26.4,25.8-26.2c14.3,0.1,25.5,11.7,25.5,26.2c0,114,0,227.9,0,341.9c0,1.2,0,2.5,0,4.8c1.6-1.5,2.6-2.4,3.5-3.3c18.3-18.3,36.5-36.6,54.8-54.8c7.1-7.1,15.8-9.4,25.4-6.7c9.5,2.7,15.5,9.3,17.9,18.8c2.6,10-1.3,18.3-8.3,25.3c-19,19-38,38-57,57c-14.5,14.5-28.9,29-43.3,43.5c-10.9,11-27,10.5-37.9-0.6c-32.8-33.1-65.8-65.9-98.6-98.9c-2-2-3.9-4.1-5.5-6.4c-7.6-10.8-5.5-25.3,4.6-33.9c9.8-8.3,25.1-7.8,34.3,1.3c18.5,18.4,36.9,36.9,55.4,55.3C99.4,386.3,100.4,387.2,102,388.7z"/></svg>
)";
const juce::String SvgIconManager::eyeDropperIcon = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" viewBox="0 0 16 16">
  <path d="M13.354.646a1.207 1.207 0 0 0-1.708 0L8.5 3.793l-.646-.647a.5.5 0 1 0-.708.708L8.293 5l-7.147 7.146A.5.5 0 0 0 1 12.5v1.793l-.854.853a.5.5 0 1 0 .708.707L1.707 15H3.5a.5.5 0 0 0 .354-.146L11 7.707l1.146 1.147a.5.5 0 0 0 .708-.708l-.647-.646 3.147-3.146a1.207 1.207 0 0 0 0-1.708zM2 12.707l7-7L10.293 7l-7 7H2z"/>
</svg>
)";

const juce::String SvgIconManager::eyeSlash = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 640 640">
<path d="M73 39.1C63.6 29.7 48.4 29.7 39.1 39.1C29.8 48.5 29.7 63.7 39 73.1L567 601.1C576.4 610.5 591.6 610.5 600.9 601.1C610.2 591.7 610.3 576.5 600.9 567.2L504.5 470.8C507.2 468.4 509.9 466 512.5 463.6C559.3 420.1 590.6 368.2 605.5 332.5C608.8 324.6 608.8 315.8 605.5 307.9C590.6 272.2 559.3 220.2 512.5 176.8C465.4 133.1 400.7 96.2 319.9 96.2C263.1 96.2 214.3 114.4 173.9 140.4L73 39.1zM236.5 202.7C260 185.9 288.9 176 320 176C399.5 176 464 240.5 464 320C464 351.1 454.1 379.9 437.3 403.5L402.6 368.8C415.3 347.4 419.6 321.1 412.7 295.1C399 243.9 346.3 213.5 295.1 227.2C286.5 229.5 278.4 232.9 271.1 237.2L236.4 202.5zM357.3 459.1C345.4 462.3 332.9 464 320 464C240.5 464 176 399.5 176 320C176 307.1 177.7 294.6 180.9 282.7L101.4 203.2C68.8 240 46.4 279 34.5 307.7C31.2 315.6 31.2 324.4 34.5 332.3C49.4 368 80.7 420 127.5 463.4C174.6 507.1 239.3 544 320.1 544C357.4 544 391.3 536.1 421.6 523.4L357.4 459.2z"/>
</svg>
)";

const juce::String SvgIconManager::eyeOpen = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 576 512">
<path d="M572.52 241.4C518.29 135.59 410.93 64 288 64S57.68 135.64 3.48 241.46a48.35 48.35 0 0 0 0 49.19C57.71 376.41 165.07 448 288 448s230.32-71.64 284.52-177.46a48.35 48.35 0 0 0 .01-49.2zM288 400c-97.05 0-184.22-55-232-144c47.78-89 134.95-144 232-144s184.22 55 232 144c-47.78 89-134.95 144-232 144zm0-272a112 112 0 1 0 112 112a112.14 112.14 0 0 0 -112-112zm0 176a64 64 0 1 1 64-64a64.07 64.07 0 0 1 -64 64z"/>
</svg>
)";

const juce::String SvgIconManager::ulBars = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512"><path d="M48 144a48 48 0 1 0 0-96 48 48 0 1 0 0 96zM192 64c-17.7 0-32 14.3-32 32s14.3 32 32 32l288 0c17.7 0 32-14.3 32-32s-14.3-32-32-32L192 64zm0 160c-17.7 0-32 14.3-32 32s14.3 32 32 32l288 0c17.7 0 32-14.3 32-32s-14.3-32-32-32l-288 0zm0 160c-17.7 0-32 14.3-32 32s14.3 32 32 32l288 0c17.7 0 32-14.3 32-32s-14.3-32-32-32l-288 0zM48 464a48 48 0 1 0 0-96 48 48 0 1 0 0 96zM96 256a48 48 0 1 0 -96 0 48 48 0 1 0 96 0z"/>
</svg>
)";

const juce::String SvgIconManager::questionSolid = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 384 512"><path d="M311.4 32l8.6 0c35.3 0 64 28.7 64 64l0 352c0 35.3-28.7 64-64 64L64 512c-35.3 0-64-28.7-64-64L0 96C0 60.7 28.7 32 64 32l8.6 0C83.6 12.9 104.3 0 128 0L256 0c23.7 0 44.4 12.9 55.4 32zM248 112c13.3 0 24-10.7 24-24s-10.7-24-24-24L136 64c-13.3 0-24 10.7-24 24s10.7 24 24 24l112 0zM192 248c16.1 0 29.2 13.1 29.2 29.2 0 8.7-3.1 13.9-6.9 17.7-4.5 4.4-10.7 7.5-16.8 9.5-14.9 5-29.5 19.3-29.5 39.5 0 13.3 10.7 24 24 24 11.5 0 21.2-8.1 23.5-19 19.2-7.1 53.7-26.3 53.7-71.8 0-42.6-34.6-77.2-77.2-77.2s-77.2 34.6-77.2 77.2c0 13.3 10.7 24 24 24s24-10.7 24-24c0-16.1 13.1-29.2 29.2-29.2zm28 168c0-15.5-12.5-28-28-28s-28 12.5-28 28 12.5 28 28 28 28-12.5 28-28z"/>
</svg>
)";

const juce::String SvgIconManager::questionCircle = R"(
<svg xmlns = "http://www.w3.org/2000/svg" viewBox = "0 0 512 512"><path d="M256 512a256 256 0 1 0 0-512 256 256 0 1 0 0 512zm0-336c-17.7 0-32 14.3-32 32 0 13.3-10.7 24-24 24s-24-10.7-24-24c0-44.2 35.8-80 80-80s80 35.8 80 80c0 47.2-36 67.2-56 74.5l0 3.8c0 13.3-10.7 24-24 24s-24-10.7-24-24l0-8.1c0-20.5 14.8-35.2 30.1-40.2 6.4-2.1 13.2-5.5 18.2-10.3 4.3-4.2 7.7-10 7.7-19.6 0-17.7-14.3-32-32-32zM224 368a32 32 0 1 1 64 0 32 32 0 1 1 -64 0z"/>
</svg>
)";

// SvgIconManager implementations

void SvgIconManager::initialise() {}

juce::String SvgIconManager::getSvgString(IconType iconType)
{
    switch (iconType)
    {
        case IconType::DragDrop:     return dragDropIcon;
        case IconType::DragDropThin: return dragDropIconThin;
        case IconType::EyeDropper:   return eyeDropperIcon;
        case IconType::Eye:          return eyeOpen;
        case IconType::EyeSlash:     return eyeSlash;
        case IconType::UlBars:       return ulBars;
        case IconType::SolidQuest:   return questionSolid;
        case IconType::QuestCirc:   return questionCircle;

        default:                     return juce::String();
    }
}

std::unique_ptr<juce::Drawable> SvgIconManager::getDrawable(IconType iconType, const juce::Component& component)
{
    juce::String svgString = getSvgString(iconType);
    if (!svgString.isEmpty())
    {
        auto drawable = juce::Drawable::createFromImageData(svgString.toRawUTF8(), svgString.getNumBytesAsUTF8());
        if (drawable)
        {
            auto iconColour = component.getLookAndFeel().findColour(juce::ListBox::textColourId);
            drawable->replaceColour(juce::Colour(0xff000000), iconColour);
            return drawable;
        }
    }
    return nullptr;
}

// ToggleIconComponent implementations
ToggleIconComponent::ToggleIconComponent(IconType offIconType, IconType onIconType)
    : juce::Button("ToggleIconComponent"),
      offIconType(offIconType), onIconType(onIconType)
{
    setClickingTogglesState(true);
}

void ToggleIconComponent::clicked() {}

juce::String ToggleIconComponent::getButtonText() const { return {}; }

void ToggleIconComponent::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto iconToDraw = getToggleState() ?
                      SvgIconManager::getDrawable(onIconType, *this) :
                      SvgIconManager::getDrawable(offIconType, *this);

    if (iconToDraw != nullptr)
    {
        // Get the base color from the LookAndFeel, which SvgIconManager already applied.
        auto iconColour = getLookAndFeel().findColour(juce::ListBox::textColourId);

        // Modify the color based on the button's state.
        if (isButtonDown)
        {
            iconColour = iconColour.darker(0.3f);
        }
        else if (isMouseOverButton)
        {
            iconColour = iconColour.brighter(0.2f);
        }
        
        // This is a crucial step. We replace the color of the drawable *before* drawing it.
        // SvgIconManager::getDrawable already replaced the original black with the default text color,
        // now we replace that default text color with our modified one.
        iconToDraw->replaceColour(getLookAndFeel().findColour(juce::ListBox::textColourId), iconColour);

        // Draw the modified icon.
        iconToDraw->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
    }
}
