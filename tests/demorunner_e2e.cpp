#include <JuceHeader.h>
#include "test_support.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>

#ifndef JUCEWRIGHT_DEMORUNNER_EXECUTABLE
    #error "JUCEWRIGHT_DEMORUNNER_EXECUTABLE must be defined"
#endif

#ifndef JUCEWRIGHT_EXECUTABLE
    #error "JUCEWRIGHT_EXECUTABLE must be defined"
#endif

namespace
{
    using namespace jucewright_test;

    constexpr const char* sessionName = "juce_demorunner";

    class DemoRunnerE2E
    {
        #include "demorunner_e2e/class_public.ipp"
        #include "demorunner_e2e/session_and_locator_helpers.ipp"
        #include "demorunner_e2e/demo_exercises.ipp"
        #include "demorunner_e2e/evidence_and_mcp.ipp"
    };
}

#include "demorunner_e2e/main.ipp"
