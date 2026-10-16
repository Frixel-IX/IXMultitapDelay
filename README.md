**What this plugin does**
===================================================
IXMultitapDelay is a JUCE based delay plugin for music composers and sound designers. Use it to add some groove or fun texture in your track!

The upper channel offers you 8 individual taps whose _timing_, _humanize range_, and _panning_ you could freely adjust; and the lower one allows gain enveloping.

Play with it and save your layouts! Presets are saved as .ixp files in (your application data directory)/IXMultitapDelay/Presets

**Downloading the VST3 Version**
=====================
You can find the zipped .vst3 file in the Releases, save it to your system's vst3 folder and it should be ready for use.

**Building from source on MacOS**
=========
JUCE is Required for this plugin to build

**1. Clone the repository**
---
   
or, select Code → Download ZIP on GitHub and extract the source.

**2. Configure the project in Projucer**
---

Open the .jucer file in Projucer.

Configure Projucer’s global JUCE modules path to point to your installation’s JUCE/modules folder.

Confirm that all listed modules resolve without errors.

Ensure VST3 is enabled in the project’s plugin formats.

Select the Xcode (macOS) exporter and save the project.

Saving generates the Builds/MacOSX and JuceLibraryCode files. These directories are intentionally excluded from Git, so they will not exist in a fresh clone until you export the project.

**3. Compile the VST3**
--

Open the generated project:

open Builds/MacOSX/IXMultitapDelay.xcodeproj

In Xcode, select the VST3 scheme, use the Release build configuration, and build.

Alternatively, run this command from the repository root:

    xcodebuild \
     -project Builds/MacOSX/IXMultitapDelay.xcodeproj \
     -target "IXMultitapDelay - VST3" \
     -configuration Release \
     build

With the default output settings, the finished plugin is located at:

Builds/MacOSX/build/Release/IxMultitapDelay.vst3

For a universal macOS build, configure the exporter to build both arm64 and x86_64.

**4. Install and test**
--

Copy the complete .vst3 bundle into:

~/Library/Audio/Plug-Ins/VST3/

