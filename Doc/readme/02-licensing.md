[← Main Index](../../README.md) | Next: [03 — Installation Guide →](03-installation.md)

---

# 02 Licensing

> **TL;DR**
> - CtrlrX is an alternative fork of Ctrlr by Roman Kubiak, aiming to provide extended community functionality.
> - Panels created inside the application remain your exclusive intellectual property.
> - Core engine code utilizes dual **BSD-3-Clause** / **GPL-2.0** licensing, but companion runtime target wrappers are tightly bound by proprietary or platform SDK terms (JUCE, AAX, VST3, AU).

## Contents
- [Licensing Terms](#licensing-terms)
- [Framework & SDK Dependencies](#framework--sdk-dependencies)
- [JUCE Framework Licensing](#juce-framework-licensing)
- [VST3 Plugin-Specific Licensing & Steinberg SDK Terms](#vst3-plugin-specific-licensing--steinberg-sdk-terms)
- [VST2 Plugin-Specific Licensing & Steinberg SDK Terms](#vst2-plugin-specific-licensing--steinberg-sdk-terms)
- [AudioUnit (AU/AUv3) Plugin-Specific Licensing & Apple SDK Terms](#audiounit-auauv3-plugin-specific-licensing--apple-sdk-terms)
- [AAX Plugin-Specific Licensing & Avid SDK Terms](#aax-plugin-specific-licensing--avid-sdk-terms)

---

## Licensing Terms

The [CtrlrX](https://github.com/damiensellier/CtrlrX) project by [Damien Sellier](https://github.com/DamienSellier/) is a fork of [Ctrlr](https://github.com/RomanKubiak/ctrlr) by [Roman Kubiak](https://github.com/RomanKubiak). The original creator has generously granted permission for derivative works (forks) to choose their own licensing terms.

Specifically, Roman Kubiak stated the following regarding the Ctrlr license:

> "It means that you can use Ctrlr code in two ways."
>
> "One is a GPL licence that means if you want to use Ctrlr’s code you must include it with your product."
>
> "The other licence means that you can do whatever you want with the code, you don’t need to publish the source code, you just need to mention where did the code come from."
>
> "Panels are not covered by this license, you can do whatever you want with them, sell them, give them away whatever, noone else has the rights to them, they are your intelectual property."
>
> [Source](https://web.archive.org/web/20240213071445/https://ctrlr.org/forums/topic/ctrlr-licence/#post-37755) (2015-02-11 11:58)

Based on this explicit permission, the core code of this plugin (CtrlrX) is licensed under the **[BSD-3-Clause License](LICENSE.BSD-3.md)** or **[GPL-2.0-or-later License](LICENSE.GPL-2.0.md)** - *choose and keep only the relevant one for your core project*.

> ⚠️ **Important Note:** Panels are explicitly **not** covered by this copyleft license architecture. You retain full rights to sell, distribute, or close the source of your layout configurations as private intellectual property.

---
## Framework & SDK Dependencies

Because CtrlrX links into distinct platform SDKs, production builds inherit additional licensing obligations:

| Dependency / Format | Licensing Architecture | Commercial Restrictions |
|---|---|---|
| **[JUCE Framework](#juce-framework-licensing)** | Dual GPLv3 / Commercial | Required for proprietary commercial plugin projects. |
| **[VST3 Format](#vst3-plugin-specific-licensing--steinberg-sdk-terms)** | Steinberg Dual GPLv3 / Commercial | Requires a separate commercial agreement signed with Steinberg. |
| **[VST2 Format](#vst2-plugin-specific-licensing--steinberg-sdk-terms)** | Discontinued Standard | Legacy builds require pre-existing license terms with Steinberg. |
| **[AudioUnit (AU/AUv3)](#audiounit-auauv3-plugin-specific-licensing--apple-sdk-terms)** | Apple Developer Terms | Bound by Apple Developer Program License Agreements. |
| **[AAX Format](#aax-plugin-specific-licensing--avid-sdk-terms)** | Avid SDK via GPLv3 | Commercial release requires specialized PACE code-signing workflows. |

---

## JUCE Framework Licensing

CtrlrX is built upon the [JUCE](https://juce.com/) C++ framework for audio applications. JUCE itself operates under a **dual-licensing model**:

* **GNU General Public License v3 (GPLv3)**: Available for open-source projects.
* **Commercial Licenses**: Required for proprietary commercial products.

Given that CtrlrX is an open-source project, it is generally built and used with JUCE under the terms of the **GPLv3**. This means that if you link CtrlrX with JUCE under its GPLv3 license, the resulting combined work (i.e., CtrlrX) becomes subject to the GPLv3's "copyleft" provisions. Consequently, any distribution of CtrlrX binaries linked with GPLv3-licensed JUCE, along with their corresponding source code, must comply with the GPLv3.

* JUCE is a registered trademark of Raw Material Software Limited acquired by Pace Anti-Piracy Inc.

### Crucial Note for Commercial Distribution of JUCE based softwares (including those based on this project)

If you plan to commercially distribute any product (including a derivative of CtrlrX) that uses the JUCE framework, you will typically need to obtain an appropriate **commercial license from JUCE**. Using JUCE under a commercial license allows you to distribute your proprietary products without being bound by the GPLv3's copyleft requirements for your own code.

For detailed and up-to-date information on JUCE's licensing, please visit the official JUCE website:
* [https://juce.com/licensing/](https://juce.com/licensing/)
* [https://juce.com/juce-legal/](https://juce.com/juce-legal/)

---

## VST3 Plugin-Specific Licensing & Steinberg SDK Terms

This plugin is designed to be compiled as a VST3 plugin. To do so, it utilizes the Steinberg VST3 SDK.

The Steinberg VST3 SDK is available under a **dual-licensing model**:
* **GNU General Public License v3 (GPLv3)**: For open-source projects.
* **Commercial License**: Required for commercial products.

For the purposes of this open-source project, we use the VST3 SDK under the terms of the **GPLv3**. Due to the "copyleft" nature of the GPLv3, any distributed VST3 plugin (including the portions of this project's code linked with the SDK) **must also be licensed under GPLv3**.

Therefore, any distributed VST3 binaries, along with their corresponding source code, are subject to the terms of the GPLv3. You can find the full text of the GPLv3 [here](https://www.gnu.org/licenses/gpl-3.0.en.html).

* VST3 is a registered trademark of Steinberg Media Technologies GmbH.

### Crucial Note for Commercial Distribution of VST3 Plugins (including those based on this project)

If you intend to commercially release a third-party plugin based on this project (or any VST3 plugin derived from the Steinberg VST3 SDK), you **must obtain a separate commercial license from Steinberg**. This commercial license typically has no runtime fees or royalties, but it is necessary for proprietary commercial distribution.

For detailed information on VST3 licensing, please refer to the official Steinberg VST 3 Licensing page:
[https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/Index.html](https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/Index.html)

---

## VST2 Plugin-Specific Licensing & Steinberg SDK Terms

The VST2 API and SDK were officially discontinued by Steinberg in January 2022. You can find more details about this decision on Steinberg's help center: [VST 2 Discontinued](https://helpcenter.steinberg.de/hc/en-us/articles/4409561018258-VST-2-Discontinued).

Due to this discontinuation, we do not officially distribute a VST2.4 version of CtrlrX. However, if you are an existing licensee of the VST2 SDK from Steinberg, you may still be able to build a VST2 version of CtrlrX for your personal use or under the terms of your specific VST2 SDK license agreement.

* VST is a registered trademark of Steinberg Media Technologies GmbH.

---

## AudioUnit (AU/AUv3) Plugin-Specific Licensing & Apple SDK Terms

This plugin can be compiled as an AudioUnit (AU) and AudioUnit v3 (AUv3) plugin for macOS and iOS respectively. Development for these formats utilizes Apple's proprietary macOS/iOS SDKs and Xcode development tools.

Unlike VST3 or AAX, there isn't a separate, redistributable "AudioUnit SDK" with a distinct open-source license. Instead, the use of AudioUnit APIs and the distribution of AU/AUv3 plugins are governed by the **Apple Developer Program License Agreement (ADPPA)** and the terms associated with the macOS and iOS Software Development Kits (SDKs).

For the purposes of this open-source project, the compilation and use of AudioUnit frameworks fall under the general terms of Apple's developer agreements. If this project's core code is licensed under GPLv3 (due to linking with GPLv3-licensed JUCE or other SDKs), then the distributed AU/AUv3 binaries, along with their corresponding source code, are subject to the terms of the GPLv3.

* Audio Unit and AUv3 are trademarks of Apple Inc., registered in the U.S. and other countries.

### Crucial Note for Commercial Distribution & App Store Submission of AU & AUv3 Plugins (including those based on this project)
If you intend to commercially distribute an AU or AUv3 plugin derived from this project, or submit it to the Mac App Store or iOS App Store, you **must be an enrolled member of the Apple Developer Program** and comply with all terms and conditions of the **Apple Developer Program License Agreement**. This includes requirements for code signing, notarization (for macOS), and adherence to App Store review guidelines.

For detailed information on Apple's developer agreements and guidelines, please refer to the official Apple Developer website:
[https://developer.apple.com/support/terms/](https://developer.apple.com/support/terms/)
[https://developer.apple.com/audio-units/](https://developer.apple.com/audio-units/)

---

## AAX Plugin-Specific Licensing & Avid SDK Terms

This plugin is designed to be compiled as an AAX plugin for Pro Tools. To do so, it utilizes the Avid AAX SDK (version 2.8.1 or newer).

The Avid AAX SDK is available under **commercial or open-source licensing**. For the purposes of this open-source project, we are using the AAX SDK under the terms of the **GNU General Public License v3 (GPLv3)**.

Due to the "copyleft" nature of the GPLv3, the entire distributed AAX plugin (including the portions of this project's code linked with the SDK) **must also be licensed under GPLv3**.

Therefore, any distributed AAX binaries, along with their corresponding source code, are subject to the terms of the GPLv3. You can find the full text of the GPLv3 [here](https://www.gnu.org/licenses/gpl-3.0.en.html).

**By building or using this plugin's AAX version, you acknowledge and agree to the terms of:**

* The [Avid AAX SDK License Agreement](https://developer.avid.com/aax)
* The [Avid Privacy Policy](https://www.avid.com/legal/privacy-policy-statement)

* AAX is a registered trademark of Avid Technologies, Inc.

### Crucial Note for Commercial Distribution of AAX Plugins (including those based on this project)

While this project is open-source under GPLv3, the Avid AAX SDK itself offers a dual-licensing model. **If you intend to commercially release a third-party plugin based on this project (or any AAX plugin derived from the Avid AAX SDK), you typically need to obtain a separate commercial license from Avid for the AAX SDK and comply with their specific commercial terms, which often include requirements for AAX plugin code signing via PACE Anti-Piracy tools.**

For information on commercial AAX development and licensing, please contact Avid directly at `audiosdk@avid.com` or visit the [Avid Developer Website](https://developer.avid.com/aax).

---

[← Main Index](../../README.md) | Next: [03 — Installation Guide →](03-installation.md)
