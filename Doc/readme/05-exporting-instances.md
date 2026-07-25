[← 04 — Compilation Guide](04-compiling.md) | [Main Index](../../README.md) | Next: [05 — Roadmap & Changelog →](06-changelog.md)

---

# 05 — Exporting Instances

> **TL;DR**
> - CtrlrX allows panels to be wrapped into standalone, fully branded DAW plugin instances directly from the interface layout engine.
> - Versions v5.6.32+ fully automate VST3 identifier mapping and host background code-signing.
> - AAX target extraction requires pre-compiling the core static SDK libraries inside your build environment.

## Contents
- [VST2 Support & Build Instructions](#vst2-support--build-instructions)
- [VST3 Support](#vst3-support)
- [AU Support](#au-support)
- [AUv3 Support](#auv3-support)
- [AAX Support & Build Instructions](#aax-support--build-instructions)

---

## VST2 Support & Build Instructions

**Important Note: VST2 Discontinuation and SDK Availability**

**We strongly recommend that users transition to building the VST3 version of CtrlrX instead of VST2, as VST3 is the current and actively supported standard.**

Steinberg officially discontinued the VST2 API and SDK in **January 2022**. As a result, the VST2 SDK is no longer publicly available for download or new licensing. This means that for most new users, building a VST2.4 plugin directly from this project will not be possible without prior access to the SDK. For more information, please refer to Steinberg's official announcement: [VST 2 Discontinued](https://helpcenter.steinberg.de/hc/en-us/articles/4409561018258-VST-2-Discontinued).

However, if you are an **existing licensee** of the VST2 SDK (meaning you had access to it prior to its discontinuation), you can still compile a VST2 version of CtrlrX. Your ability to build and distribute this VST2.4 version of the plugin will be governed solely by the terms of your specific VST2 SDK license agreement with Steinberg.

**To build the VST2 version of this plugin (for existing licensees):**

1.  **Ensure VST2 SDK Availability:**
    Make sure you have your VST2 SDK files (specifically the `vst2.x` folder containing headers like `aeffect.h`) accessible on your system. This SDK is **not included** with JUCE and must be sourced from your prior access.

2.  **Configure JUCE Projucer Global Paths:**
    * Open the JUCE Projucer application (compatible with your JUCE 6.0.8 version).
    * Navigate to **"File" > "Global Paths..."** (or sometimes just "Settings" via the gear icon).
    * In the Global Paths window, locate the entry specifically for **"VST (Legacy) SDK"** or "VST2 SDK".
    * Set this path to the root directory where your VST2 SDK is located (e.g., the folder containing the `pluginterfaces` directory and other VST2 SDK components). This path is critical for JUCE to find the necessary VST2 headers and libraries.
    * Open your CtrlrX `.jucer` project file in Projucer, go to the "Exporters" section, and ensure the VST2 format is enabled for your target platform.

3.  **Build the CtrlrX VST2 binary:**
    Follow your project's general build instructions (e.g., via your IDE after Projucer generation, or directly with CMake) to compile the VST2 target.

---

## VST3 Support

To compile the VST3 version of this plugin, ensure your **JUCE installation is correctly set up to include the VST3 SDK**, as it's typically bundled with JUCE.

### UPDATE v5.6.32+

VST3 exported instances of CtrlrX panels are finally working properly. On macOS, you need to export VST3 with at least an ad-hoc signature. This process is managed automatically by CtrlrX; if you have an Apple developer certificate, select it in the appropriate field to prevent the VST3 from being held by Gatekeeper. If you want to sign your Windows exported instances with a PFX Certificate, that's now also possible.

If for some reason you still want to produce your own VST3 intermediates, you can simply disable the unique identifiers replacement process from the appropriate field.

### Prior v5.6.32

Currently, VST3 instances of Ctrlr panels are not working properly because Ctrlr is not able to generate different VST3 compliant plugin identifiers.

Unfortunately, exported VST3 instances of your panel will always be named after CTRLR | Instigator. The only way to get the correct identifiers for a panel project is to force them at the core during the building step of the VST3 in Xcode/VS/IDE.
To export properly identified VST3 plugins, it is then required to build a different stock CTRLR VST3 plugin with JUCE Projucer and Xcode/VS/IDE.
However, this alternative version of CTRLR VST3 will have the desired panel/plugin identifiers predefined in the Projucer settings (Plugin Name, Manufacturer Name, Plugin ID, Manufacturer ID, etc.).
This intermediate VST3 plugin will then be able to export a final VST3 version of the panel with the proper identifiers.

---

## AU Support

To compile the AU & AUv3 versions of this plugin, ensure your **JUCE installation is correctly set up to include the AU SDK**, as it's typically bundled with JUCE.

Tutorial coming soon.

---

## AUv3 Support

To compile the VST3 version of this plugin, ensure your **JUCE installation is correctly set up to include the AUv3 SDK**, as it's typically bundled with JUCE.

Tutorial coming soon.

---

## AAX Support & Build Instructions

This project is built and tested with **JUCE 6.0.8**. To compile the AAX version of this plugin, you will also need to obtain the **Avid AAX SDK (version 2.8.1 or newer)** separately.

**To build the AAX version of this plugin:**

1.  **Obtain the Avid AAX SDK 2.8.1 (or newer compatible version):**
    * **Join the Avid Developer Program:** Visit the [Avid Developer website](https://developer.avid.com/aax/) and sign up for the AAX SDK program. This typically involves agreeing to their terms and creating an iLok account.
    * **Download the AAX SDK:** Once registered, navigate to the SDK download section and download "AAX SDK 2.8.1" (or the latest GPLv3-licensed version available for use with JUCE 6.0.8).
    * **Extract the SDK:** Extract the downloaded SDK archive to a stable, known location on your system (e.g., `C:\SDKs\AvidAAXSDK_2.8.1\` on Windows or `~/SDKs/AvidAAXSDK_2.8.1/` on macOS/Linux).

2.  **Build the AAX Library:**
    * Before building CtrlrX, you must compile the core AAX Library project included within the SDK. Navigate to the `Libs/AAXLibrary` directory within your extracted AAX SDK and build the project using your chosen IDE (e.g., Visual Studio on Windows, Xcode on macOS). This step generates the necessary static library (`.lib` or `.a`) that CtrlrX will link against.

3.  **Configure your build environment:**
    * **If using JUCE's Projucer (recommended for JUCE 6.0.8):**
        * Open the Projucer application (from your JUCE 6.0.8 clone).
        * Go to **"Settings"** (gear icon or File > Global Paths).
        * Under "SDK Locations" or "Module Paths", locate the entry for **"AAX SDK"** and set its path to the root directory of your extracted Avid AAX SDK (e.g., `C:\SDKs\AvidAAXSDK_2.8.1\`).
        * Open your `.jucer` project file, go to the "Exporters" section, and ensure the AAX format is enabled for your target platform.
    * **If using CMake:**
        * Your `CMakeLists.txt` will need to be configured to locate both your JUCE 6.0.8 root and the AAX SDK root.
        * You'll typically define variables (e.g., `JUCE_ROOT`, `AAX_SDK_ROOT`) and use `find_path` or explicit `set` commands, then pass `AAX_SDK_ROOT` to `juce_add_plugin` via the `AAX_SDK_PATH` argument. (Detailed CMake setup for JUCE 6.0.8 with external SDKs can be complex; a dedicated CMake guide might be needed for this if it's your primary build method).

4.  **Build the CtrlrX AAX binary:** Follow your project's general build instructions (e.g., via your IDE after Projucer generation, or directly with CMake) to compile the AAX target.
    * Open the .jucer project in Projucer, select the AAX target, and click 'Save and Open in IDE'. Then build in your IDE. Alternatively for CMake: `cmake --build . --config Release --target CtrlrX_AAX`.

**Additional Notes for Development and Distribution:**

* **Pro Tools Developer Version:** To test unsigned AAX plugins, you will likely need a special "Pro Tools Developer" version of Pro Tools, available through the [Avid Developer Program](https://developer.avid.com/aax).

* **PACE Codesigning:** For any distributed AAX plugin (commercial or free), you will generally need to go through the AVID authorization process via PACE codesigning tools. This is a requirement for plugins to load in standard (non-developer) Pro Tools versions. This process is managed by Avid and PACE.

---

[← 03 — Compilation Guide](04-compiling.md) | [Main Index](../../README.md) | Next: [05 — Roadmap & Changelog →](06-changelog.md)
