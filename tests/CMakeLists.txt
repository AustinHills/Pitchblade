# Project name for tests
project(PitchbladeTests)


# RubberBand configuration==============================================
set(RUBBERBAND_DIR ${CMAKE_SOURCE_DIR}/plugin/third-party/rubberband)
set(RUBBERBAND_INCLUDE_DIR ${RUBBERBAND_DIR}/include/)

# Select Release or Debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(RUBBERBAND_LIBRARY ${RUBBERBAND_DIR}/lib/x64/Debug/rubberband-library.lib)
else()
    set(RUBBERBAND_LIBRARY ${RUBBERBAND_DIR}/lib/x64/Release/rubberband-library.lib)
endif()

# Import RubberBand as an interface library
add_library(RubberBand STATIC IMPORTED)
set_target_properties(RubberBand PROPERTIES
    IMPORTED_LOCATION ${RUBBERBAND_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${RUBBERBAND_INCLUDE_DIR}
)

# Verify the library exists
if(NOT EXISTS ${RUBBERBAND_LIBRARY})
    message(WARNING "RubberBand library not found at: ${RUBBERBAND_LIBRARY}")
endif()

# Create the test executable
add_executable(runTests
    # List all your test files here as you create them
    test_main.cpp
    test_GainProcessor.cpp
    test_PitchDetector.cpp
    test_PitchCorrector.cpp
    test_NoiseGateProcessor.cpp
    test_CompressorProcessor.cpp
    test_DeEsserProcessor.cpp
    test_DeNoiserProcessor.cpp
    test_UI_DaisyChain.cpp
    test_FormantShifter.cpp
    test_FormantDetector.cpp
    test_Equalizer.cpp
    test_Integration_Amplitude.cpp
    test_Integration_Performance.cpp
    test_Integration_Formant.cpp
    test_Integration_Equalizer.cpp
    test_Integration_PitchCorrector.cpp
    test_Integration_VisualizerPanel.cpp
 )

# This forces CMake to build the 'Pitchblade' target (and generate JuceHeader.h)
# *before* it tries to build 'runTests'. This fixes the "JuceHeader.h not found" error.
add_dependencies(runTests Pitchblade BinaryData)

# Link Google Test
target_link_libraries(runTests
    PRIVATE
        gmock_main
)

# --- Link your plugin's main processor ---
target_link_libraries(runTests
    PRIVATE
        # This will bring in the main processor (Pitchblade)
        # and all its dependencies (panels, effects, etc.)
        Pitchblade

        # Linking to BinaryData (which is an INTERFACE library)
        # automatically adds the correct include path for "BinaryData.h"
        BinaryData

        # Include rubber band for pitch correction
        RubberBand
)

# Include directories
target_include_directories(runTests
    PRIVATE
        # We MUST use CMAKE_SOURCE_DIR, not PROJECT_SOURCE_DIR
        ${CMAKE_SOURCE_DIR}/plugin/include
        
        # This is created by JUCE and needed by PluginProcessor.h
        ${CMAKE_BINARY_DIR}/plugin/Pitchblade_artefacts/JuceLibraryCode
        
        # This path is for the auto-generated "BinaryData.h"
        ${CMAKE_BINARY_DIR}/plugin/Pitchblade_artefacts/BinaryData
)

# --- CTest Integration ---
# Automatically discover all tests in your executable
include(GoogleTest)
gtest_discover_tests(runTests)
