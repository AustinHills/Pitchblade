# Project name for tests
project(PitchbladeTests)


# RubberBand configuration==============================================
# RubberBand configuration==============================================
if(NOT TARGET RubberBand)
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

        # Verify the library exists
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
        
        message(STATUS "Found RubberBand (Tests): ${RUBBERBAND_LIBRARIES}")
    endif()
endif()

# Create the test executable
add_executable(runTests
    # List all your test files here as you create them
    test_main.cpp
    test_AutoGainProcessor.cpp
    test_GainProcessor.cpp
    test_PitchDetector.cpp
    test_PitchCorrector.cpp
    test_NoiseGateProcessor.cpp
    test_CompressorProcessor.cpp
    test_DeEsserProcessor.cpp
    test_DeNoiserProcessor.cpp
    test_SaturationProcessor.cpp
    test_UI_DaisyChain.cpp
    test_BandPassCompressorProcessor.cpp
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
