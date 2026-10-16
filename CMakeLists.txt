cmake_minimum_required(VERSION 3.22)

project(MadScaler VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MADSCALER_BUILD_PLUGIN "Build the JUCE plugin" ON)
option(MADSCALER_BUILD_TESTS "Build the music theory unit tests" ON)

# Music theory core: plain C++, no JUCE, so it can be unit tested anywhere.
add_library(madscaler_core STATIC Source/ChordEngine.cpp Source/ChordDetector.cpp Source/Voicing.cpp Source/PatternEngine.cpp Source/Progression.cpp Source/ChordColours.cpp)
target_include_directories(madscaler_core PUBLIC Source)
set_target_properties(madscaler_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MADSCALER_BUILD_TESTS)
    enable_testing()
    add_executable(madscaler_tests tests/ChordEngineTests.cpp)
    target_link_libraries(madscaler_tests PRIVATE madscaler_core)
    add_test(NAME chord_engine COMMAND madscaler_tests)

    add_executable(madscaler_detector_tests tests/ChordDetectorTests.cpp)
    target_link_libraries(madscaler_detector_tests PRIVATE madscaler_core)
    add_test(NAME chord_detector COMMAND madscaler_detector_tests)

    add_executable(madscaler_voicing_tests tests/VoicingTests.cpp)
    target_link_libraries(madscaler_voicing_tests PRIVATE madscaler_core)
    add_test(NAME voicing COMMAND madscaler_voicing_tests)

    add_executable(madscaler_pattern_tests tests/PatternEngineTests.cpp)
    target_link_libraries(madscaler_pattern_tests PRIVATE madscaler_core)
    add_test(NAME pattern_engine COMMAND madscaler_pattern_tests)

    add_executable(madscaler_progression_tests tests/ProgressionTests.cpp)
    target_link_libraries(madscaler_progression_tests PRIVATE madscaler_core)
    add_test(NAME progression COMMAND madscaler_progression_tests)

    add_executable(madscaler_colour_tests tests/ColourTests.cpp)
    target_link_libraries(madscaler_colour_tests PRIVATE madscaler_core)
    add_test(NAME colours COMMAND madscaler_colour_tests)
endif()

if(MADSCALER_BUILD_PLUGIN)
    # JUCE is downloaded at configure time. To use a local checkout instead, pass
    # -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(MadScaler
        COMPANY_NAME "MadScaler"
        BUNDLE_ID "com.madscaler.madscaler"
        PLUGIN_MANUFACTURER_CODE Mdsc
        PLUGIN_CODE Mdsc
        PRODUCT_NAME "MadScaler"
        FORMATS VST3 Standalone
        # An instrument with MIDI in and MIDI out. It makes no sound itself:
        # its MIDI output is routed to another synth in the DAW.
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES Instrument Generator
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(MadScaler PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(MadScaler PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(MadScaler
        PRIVATE
            madscaler_core
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    if(MADSCALER_BUILD_TESTS)
        add_executable(madscaler_processor_tests tests/ProcessorTests.cpp)
        # Reuses the plugin's compiled JUCE code, so only its headers and flags are needed here.
        target_include_directories(madscaler_processor_tests PRIVATE
            $<TARGET_PROPERTY:MadScaler,INCLUDE_DIRECTORIES>)
        target_compile_definitions(madscaler_processor_tests PRIVATE
            $<TARGET_PROPERTY:MadScaler,COMPILE_DEFINITIONS>)
        target_link_libraries(madscaler_processor_tests PRIVATE MadScaler madscaler_core)
        add_test(NAME processor COMMAND madscaler_processor_tests)
    endif()
endif()
