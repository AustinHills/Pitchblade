# Pitchblade

***Pitchblade*** is a vocal multi-tool audio plugin built with C++23 and the JUCE framework. It is designed for comprehensive vocal processing, and it combines many tools into a singular, modular interface.

The plugin features a dynamic "Daisy Chain" signal flow, allowing users to reorder effects on the fly, alongside real-time visualizers for precise monitoring.

## Features

Pitchblade includes several processing modules tailored for vocals:
* **Pitch Correction and Shifting:** Real-time pitch detection and correction (powered by RubberBand)
* **Formant Manipulation:** Formant detection and shifting for altering vocal timbre without affecting pitch (powered by RubberBand)
* **Gain:** Increase or decrease volume.
* **Auto Gain:** Automatically adjusts the volume to maintain a consistent target level.
* **Compressor:** Controls dynamic range, making louder sounds quieter. Also includes a limiter.
* **BandPass Compressor:** Compresses audio only within a specific frequency range.
* **Noise Gate:** Eliminates sounds below a certain level.
* **De-Esser:** Reduces harsh sibilance.
* **De-Noiser:** Learns what sounds are present in the background to reduce them. Recommended to be used with the Noise Gate for full noise reduction.
* **Saturation:** Adds harmonic saturation and warmth to the signal.
* **Equalizer:** Increases or reduces amplitude at specific frequencies to shape the sound of a voice.
* **VST3 Hosting:** Allows loading external VST3 plugins anywhere in the signal chain.

Pitchblade also includes a few quality-of-life features to make the user experience better:
* **Visualizers:** Each panel has its own specialized visualizer based on two visualizer template classes.
* **Preset Management:** Save and load custom effect chains.

## Prerequisites

Before building, ensure you have the following installed:
* **CMake:** Version 3.22 or higher (Can be found at https://cmake.org/download/)
* **C++ Compiler:** Must support **C++23** (e.g., Visual Studio 2022 v17.x on Windows).
* **Git:** For version control.
* **NSIS:** Version 3.11 for installation script creation (https://nsis.sourceforge.io/Main_Page)
* *Note: JUCE and GoogleTest dependencies are automatically handled via the specific CMake configuration.*

## Build Instructions

### Windows

You can build the project using the provided helper script.

To generate the project files and build the solution completely (Debug mode):
```bat
.\configure_windows.bat
```

To run the test cases (Debug mode):
```bat
.\configure_windows_test.bat
```

To generate the project files and build the solution completely with an installer exe (Release mode):
```bat
.\configure_windows_release.bat
```


After compilation is complete, the compiled VST3 and Standalone executable can be found in ./build/plugin/Pitchblade_artefacts/. Alternatively, the installer script can be found in the root folder.

### Linux

You can build the project using the provided helper set.

To generate the project files and build the solution completely (Debug mode):
```bash
sh ./configure_linux.sh
```

To generate the project files and build the solution completely (Release mode):
```bash
sh ./configure_linux_release.sh
```

To run the test cases (Debug mode):
```bash
sh ./configure_linux_test.sh
```

After compilation is complete, the compiled VST3 and Standalone executable can be found in `./build/plugin/Pitchblade_artefacts/Debug/` (or `RelWithDebInfo/` if you built in Release mode).

For the Standalone App:
```bash
./build/plugin/Pitchblade_artefacts/Debug/Standalone/Pitchblade
```

For the VST3 Plugin:
Copy the `.vst3` directory to your user VST3 folder:
```bash
mkdir -p ~/.vst3
cp -r build/plugin/Pitchblade_artefacts/Debug/VST3/Pitchblade.vst3 ~/.vst3/
```

For specific instructions on the use of the plugin, please view the User Manual present in "Pitchblade User Manual.pdf".
