# Specify minimum CMake version supported. Project tested on 4.1.1
cmake_minimum_required(VERSION 3.22)

# Project name
project(Pitchblade VERSION 5.0.1)

# RubberBand configuration==============================================
if(WIN32)
    set(RUBBERBAND_DIR ${CMAKE_SOURCE_DIR}/plugin/third-party/rubberband)
    set(RUBBERBAND_INCLUDE_DIR ${RUBBERBAND_DIR}/include/)

    set(RUBBERBAND_LIBRARY_DEBUG ${RUBBERBAND_DIR}/lib/x64/Debug/rubberband-library.lib)
    set(RUBBERBAND_LIBRARY_RELEASE ${RUBBERBAND_DIR}/lib/x64/Release/rubberband-library.lib)

    # Import RubberBand as an interface library
    add_library(RubberBand STATIC IMPORTED)
    set_target_properties(RubberBand PROPERTIES
        IMPORTED_LOCATION_DEBUG ${RUBBERBAND_LIBRARY_DEBUG}
        IMPORTED_LOCATION_RELEASE ${RUBBERBAND_LIBRARY_RELEASE}
        IMPORTED_LOCATION_RELWITHDEBINFO ${RUBBERBAND_LIBRARY_RELEASE}
        IMPORTED_LOCATION_MINSIZEREL ${RUBBERBAND_LIBRARY_RELEASE}
        INTERFACE_INCLUDE_DIRECTORIES ${RUBBERBAND_INCLUDE_DIR}
    )

    # Verify the library exists (Check Debug as a proxy or check both)
    if(NOT EXISTS ${RUBBERBAND_LIBRARY_RELEASE})
        message(WARNING "RubberBand library (Release) not found at: ${RUBBERBAND_LIBRARY_RELEASE}")
    endif()

elseif(UNIX AND NOT APPLE) # Linux
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RUBBERBAND REQUIRED rubberband)

    # Create an ALIAS library so we can link against 'RubberBand' consistently
    # PkgConfig creates a target PkgConfig::RUBBERBAND if IMPORTED_TARGET is used, but manual is safer here
    add_library(RubberBand INTERFACE)
    target_include_directories(RubberBand INTERFACE ${RUBBERBAND_INCLUDE_DIRS})
    target_link_libraries(RubberBand INTERFACE ${RUBBERBAND_LIBRARIES})
    
    message(STATUS "Found RubberBand: ${RUBBERBAND_LIBRARIES}")
endif()

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
file(MAKE_DIRECTORY ${LIB_DIR})

# JUCE configuration====================================================
find_package(JUCE CONFIG REQUIRED)

# Plugin configurtion data
juce_add_plugin(${PROJECT_NAME}
    

    # VERSION ...                               # Set this if the plugin version is different to the project version
    ICON_BIG "assets/pb_logo.png"
    ICON_SMALL "assets/pb_logo.png"
    COMPANY_NAME "WSU PITCHBLADE TEAM"          # Specify the name of the plugin's author
    BUNDLE_ID "com.WSUPitchbladeTeam.Pitchblade"    #for mac functionality, ios hosts will reject bundle IDs with spaces
    IS_SYNTH FALSE                              # Is this a synth or an effect?
    NEEDS_MIDI_INPUT FALSE                      # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE                     # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                        # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE            # Does the editor need keyboard focus?
    # COPY_PLUGIN_AFTER_BUILD TRUE/FALSE        # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE Pbld               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Pbld                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME "Pitchblade"                   # The name of the final executable, which can differ from the target name
)
juce_generate_juce_header(${PROJECT_NAME})  #will generate JuceHeader.h automatically in the JuceLibraryCode folder

target_sources(${PROJECT_NAME}
    PRIVATE
        source/PluginEditor.cpp
        source/PluginProcessor.cpp
        
        #ui
        source/ui/TopBar.cpp
        source/ui/DaisyChain.cpp
        source/ui/EffectPanel.cpp
        source/ui/VisualizerPanel.cpp
        source/ui/LevelMeter.cpp
        source/ui/RealTimeGraphVisualizer.cpp
        source/ui/FrequencyGraphVisualizer.cpp
        source/ui/EqualizerVisualizer.cpp
        source/ui/FormantVisualizer.cpp
        source/ui/ColorPalette.cpp
        source/panels/PresetsPanel.cpp
        

        #remember to add your panel and processors here
        source/panels/GainPanel.cpp
        source/panels/NoiseGatePanel.cpp
        source/panels/FormantPanel.cpp
        source/panels/PitchPanel.cpp
        source/panels/CompressorPanel.cpp
        source/panels/BandPassCompressorPanel.cpp
        source/panels/DeEsserPanel.cpp
        source/panels/DeNoiserPanel.cpp
        source/panels/SettingsPanel.cpp
        source/panels/EqualizerPanel.cpp
        source/panels/VST3Panel.cpp
        source/panels/AdaptiveDeNoiserPanel.cpp
        source/panels/SaturationPanel.cpp
        source/panels/AutoGainPanel.cpp
        source/panels/EffectNode.cpp

        source/effects/AutoGainProcessor.cpp
        source/effects/GainProcessor.cpp
        source/effects/NoiseGateProcessor.cpp
        source/effects/FormantDetector.cpp
        source/effects/PitchDetector.cpp
        source/effects/PitchCorrector.cpp
        source/effects/PitchShifter.cpp
        source/effects/CompressorProcessor.cpp
        source/effects/BandPassCompressorProcessor.cpp
        source/effects/FormantShifter.cpp
        source/effects/DeEsserProcessor.cpp
        source/effects/DeNoiserProcessor.cpp
        source/effects/Equalizer.cpp
        source/effects/AdaptiveDeNoiserProcessor.cpp
        source/effects/SaturationProcessor.cpp

)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/Pitchblade_artefacts/JuceLibraryCode
)

# for adding pictures and assets
juce_add_binary_data(BinaryData
    SOURCES
        assets/pitchblade_logo.png
        assets/pb_logo.png
        assets/tooltips.txt
        assets/panel_bg.png
)

#Include these libraries in build
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        BinaryData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags     # Config flags
        juce::juce_recommended_lto_flags        # Link time optimization flags
        juce::juce_recommended_warning_flags    # Warning flags
        juce::juce_core
        juce::juce_dsp                          #dsp files
        RubberBand                              # Pitch config
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME}
    PRIVATE 
        winmm
        ws2_32
    )
endif()

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_PLUGINHOST_VST3=1
)

if(MSVC)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
        _CRT_SECURE_NO_WARNINGS
)
endif()