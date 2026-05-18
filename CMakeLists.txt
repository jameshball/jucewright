cmake_minimum_required(VERSION 3.20)

project(Jucewright VERSION 0.1.0 LANGUAGES C CXX
    DESCRIPTION "Playwright-style automation for JUCE apps and plugins"
    HOMEPAGE_URL "https://github.com/jameshball/jucewright")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

include(FetchContent)

if (Jucewright_IS_TOP_LEVEL)
    set(JUCEWRIGHT_JUCE_GIT_TAG "develop" CACHE STRING "JUCE git tag, branch, or commit used by the standalone Jucewright build")
    option(JUCEWRIGHT_BUILD_CLI "Build the jucewright CLI/MCP executable" ON)
    option(JUCEWRIGHT_BUILD_TESTS "Build Jucewright fixture tests" ON)
    option(JUCEWRIGHT_BUILD_DEMORUNNER_AUTOMATION "Build a generated JUCE DemoRunner automation target" OFF)

    message(STATUS "Fetching JUCE ${JUCEWRIGHT_JUCE_GIT_TAG}...")
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG "${JUCEWRIGHT_JUCE_GIT_TAG}"
        GIT_PROGRESS TRUE)

    FetchContent_MakeAvailable(JUCE)
endif ()

if (NOT COMMAND juce_add_module)
    message(FATAL_ERROR "JUCE must be added to your project before jucewright")
endif ()

juce_add_module("${CMAKE_CURRENT_LIST_DIR}/jucewright")
add_library(Jucewright::Jucewright ALIAS jucewright)

if (Jucewright_IS_TOP_LEVEL AND JUCEWRIGHT_BUILD_CLI)
    juce_add_console_app(jucewright_cli VERSION 0.1.0 PRODUCT_NAME "jucewright")
    target_sources(jucewright_cli PRIVATE tools/jucewright.cpp)
    set_target_properties(jucewright_cli PROPERTIES OUTPUT_NAME jucewright)
    target_compile_definitions(jucewright_cli PRIVATE
        JUCE_USE_CURL=0
        JUCE_DISABLE_JUCE_VERSION_PRINTING=1)
    target_link_libraries(jucewright_cli PRIVATE
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif ()

if (Jucewright_IS_TOP_LEVEL AND JUCEWRIGHT_BUILD_TESTS)
    if (NOT TARGET jucewright_cli)
        message(FATAL_ERROR "JUCEWRIGHT_BUILD_TESTS requires JUCEWRIGHT_BUILD_CLI=ON")
    endif ()

    juce_add_gui_app(automation_fixture VERSION 1.0.0)
    juce_generate_juce_header(automation_fixture)
    target_sources(automation_fixture PRIVATE tests/automation_fixture.cpp)
    target_compile_definitions(automation_fixture PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCEWRIGHT_ENABLE_AUTOMATION=1)
    target_link_libraries(automation_fixture PRIVATE
        jucewright
        juce::juce_gui_extra
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
    add_dependencies(automation_fixture jucewright_cli)
endif ()

if (Jucewright_IS_TOP_LEVEL AND JUCEWRIGHT_BUILD_DEMORUNNER_AUTOMATION)
    if (NOT TARGET jucewright_cli)
        message(FATAL_ERROR "JUCEWRIGHT_BUILD_DEMORUNNER_AUTOMATION requires JUCEWRIGHT_BUILD_CLI=ON")
    endif ()

    set(JUCEWRIGHT_DEMORUNNER_SOURCE_DIR "${JUCE_SOURCE_DIR}/examples/DemoRunner")
    set(JUCEWRIGHT_DEMORUNNER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/jucewright_demorunner")
    file(MAKE_DIRECTORY "${JUCEWRIGHT_DEMORUNNER_GENERATED_DIR}/Source")
    file(MAKE_DIRECTORY "${JUCEWRIGHT_DEMORUNNER_GENERATED_DIR}/Source/Demos")

    file(READ "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source/Main.cpp" JUCEWRIGHT_DEMORUNNER_MAIN)
    string(REPLACE
        "#include <JuceHeader.h>"
        "#include <JuceHeader.h>\n#include <jucewright/jucewright.h>"
        JUCEWRIGHT_DEMORUNNER_MAIN
        "${JUCEWRIGHT_DEMORUNNER_MAIN}")
    string(REPLACE
        "setContentOwned (new MainComponent(), false);"
        "auto* mainComponent = new MainComponent();\n            setContentOwned (mainComponent, false);\n\n           #if JUCEWRIGHT_ENABLE_AUTOMATION\n            jucewright::AutomationOptions automationOptions;\n            automationOptions.sessionName = \"juce_demorunner\";\n            automationOptions.allowFileWrite = true;\n            automationOptions.artifactRoot = File::getSpecialLocation (File::tempDirectory).getChildFile (\"jucewright-demorunner\");\n            automation.enable (*mainComponent, automationOptions);\n           #endif"
        JUCEWRIGHT_DEMORUNNER_MAIN
        "${JUCEWRIGHT_DEMORUNNER_MAIN}")
    string(REPLACE
        "    private:\n       #if JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX || JUCE_BSD"
        "    private:\n        jucewright::Automation automation;\n\n       #if JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX || JUCE_BSD"
        JUCEWRIGHT_DEMORUNNER_MAIN
        "${JUCEWRIGHT_DEMORUNNER_MAIN}")
    string(FIND "${JUCEWRIGHT_DEMORUNNER_MAIN}" "automationOptions.sessionName = \"juce_demorunner\"" JUCEWRIGHT_DEMORUNNER_SESSION_PATCH)
    string(FIND "${JUCEWRIGHT_DEMORUNNER_MAIN}" "jucewright::Automation automation;" JUCEWRIGHT_DEMORUNNER_MEMBER_PATCH)
    if (JUCEWRIGHT_DEMORUNNER_SESSION_PATCH EQUAL -1 OR JUCEWRIGHT_DEMORUNNER_MEMBER_PATCH EQUAL -1)
        message(FATAL_ERROR "JUCE DemoRunner automation patch failed: Source/Main.cpp patch anchors changed")
    endif ()
    file(WRITE "${JUCEWRIGHT_DEMORUNNER_GENERATED_DIR}/Source/Main.cpp" "${JUCEWRIGHT_DEMORUNNER_MAIN}")

    file(READ "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source/Demos/JUCEDemos.cpp" JUCEWRIGHT_DEMORUNNER_DEMOS)
    string(REPLACE
        "#include \"../../../Assets/DemoUtilities.h\""
        "#include \"${JUCE_SOURCE_DIR}/examples/Assets/DemoUtilities.h\""
        JUCEWRIGHT_DEMORUNNER_DEMOS
        "${JUCEWRIGHT_DEMORUNNER_DEMOS}")
    string(REPLACE
        "    auto* currentDevice = sharedAudioDeviceManager->getCurrentAudioDevice();"
        "   #if JUCEWRIGHT_DEMORUNNER_SKIP_AUDIO_DEVICE_INITIALISE\n    ignoreUnused (numInputChannels, numOutputChannels);\n    return *sharedAudioDeviceManager;\n   #else\n\n    auto* currentDevice = sharedAudioDeviceManager->getCurrentAudioDevice();"
        JUCEWRIGHT_DEMORUNNER_DEMOS
        "${JUCEWRIGHT_DEMORUNNER_DEMOS}")
    string(REPLACE
        "    return *sharedAudioDeviceManager;\n}"
        "   #endif\n\n    return *sharedAudioDeviceManager;\n}"
        JUCEWRIGHT_DEMORUNNER_DEMOS
        "${JUCEWRIGHT_DEMORUNNER_DEMOS}")
    string(FIND "${JUCEWRIGHT_DEMORUNNER_DEMOS}" "JUCEWRIGHT_DEMORUNNER_SKIP_AUDIO_DEVICE_INITIALISE" JUCEWRIGHT_DEMORUNNER_AUDIO_PATCH)
    if (JUCEWRIGHT_DEMORUNNER_AUDIO_PATCH EQUAL -1)
        message(FATAL_ERROR "JUCE DemoRunner automation patch failed: Demos/JUCEDemos.cpp patch anchors changed")
    endif ()
    file(WRITE "${JUCEWRIGHT_DEMORUNNER_GENERATED_DIR}/Source/Demos/JUCEDemos.cpp" "${JUCEWRIGHT_DEMORUNNER_DEMOS}")

    juce_add_gui_app(jucewright-demorunner
        BUNDLE_ID                       com.jucewright.demorunner
        ICON_BIG                        "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source/JUCEAppIcon.png"
        ICON_COMPOSER_BUNDLE            "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source/AppIcon.icon"
        NEEDS_CURL                      TRUE
        NEEDS_WEB_BROWSER               TRUE
        MICROPHONE_PERMISSION_ENABLED   TRUE
        CAMERA_PERMISSION_ENABLED       TRUE
        BLUETOOTH_PERMISSION_ENABLED    TRUE
        FILE_SHARING_ENABLED            TRUE
        DOCUMENT_BROWSER_ENABLED        TRUE
        REQUIRES_FULL_SCREEN            FALSE)

    juce_generate_juce_header(jucewright-demorunner)

    target_sources(jucewright-demorunner PRIVATE
        "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source/Demos/DemoPIPs1.cpp"
        "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source/Demos/DemoPIPs2.cpp"
        "${JUCEWRIGHT_DEMORUNNER_GENERATED_DIR}/Source/Demos/JUCEDemos.cpp"
        "${JUCEWRIGHT_DEMORUNNER_GENERATED_DIR}/Source/Main.cpp"
        "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source/UI/DemoContentComponent.cpp"
        "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source/UI/MainComponent.cpp")

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set_source_files_properties("${JUCEWRIGHT_DEMORUNNER_GENERATED_DIR}/Source/Demos/JUCEDemos.cpp"
            PROPERTIES COMPILE_OPTIONS "-Wno-unused-function")
    endif ()

    target_include_directories(jucewright-demorunner PRIVATE
        "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source"
        "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/Source/Demos")

    target_compile_definitions(jucewright-demorunner PRIVATE
        JUCEWRIGHT_ENABLE_AUTOMATION=1
        PIP_JUCE_EXAMPLES_DIRECTORY_STRING="${JUCE_SOURCE_DIR}/examples"
        JUCE_ASIO=1
        JUCE_ALLOW_STATIC_NULL_VARIABLES=0
        JUCE_CONTENT_SHARING=1
        JUCE_DEMO_RUNNER=1
        JUCE_PLUGINHOST_AU=1
        JUCE_PLUGINHOST_LV2=1
        JUCE_PLUGINHOST_VST3=1
        JUCE_PUSH_NOTIFICATIONS=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_UNIT_TESTS=1
        JUCE_USE_CAMERA=1
        JUCE_USE_CURL=1
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_WEB_BROWSER=1
        JUCEWRIGHT_DEMORUNNER_SKIP_AUDIO_DEVICE_INITIALISE=1
        JUCE_SILENCE_XCODE_15_LINKER_WARNING=1)

    target_link_libraries(jucewright-demorunner PRIVATE
        jucewright
        juce::juce_analytics
        juce::juce_animation
        juce::juce_audio_utils
        juce::juce_box2d
        juce::juce_build_tools
        juce::juce_dsp
        juce::juce_javascript
        juce::juce_midi_ci
        juce::juce_opengl
        juce::juce_osc
        juce::juce_product_unlocking
        juce::juce_video
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

    foreach(folder IN ITEMS ../Assets ../Audio ../DSP ../GUI ../Utilities)
        juce_add_bundle_resources_directory(jucewright-demorunner "${JUCEWRIGHT_DEMORUNNER_SOURCE_DIR}/${folder}")
    endforeach()

    juce_add_console_app(jucewright-demorunner-e2e VERSION 1.0.0)
    juce_generate_juce_header(jucewright-demorunner-e2e)
    target_sources(jucewright-demorunner-e2e PRIVATE tests/demorunner_e2e.cpp)
    target_compile_definitions(jucewright-demorunner-e2e PRIVATE
        JUCE_USE_CURL=0
        JUCEWRIGHT_DEMORUNNER_EXECUTABLE="$<TARGET_FILE:jucewright-demorunner>"
        JUCEWRIGHT_EXECUTABLE="$<TARGET_FILE:jucewright_cli>")
    target_link_libraries(jucewright-demorunner-e2e PRIVATE
        juce::juce_core
        juce::juce_graphics
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
    add_dependencies(jucewright-demorunner-e2e jucewright-demorunner jucewright_cli)
endif ()
