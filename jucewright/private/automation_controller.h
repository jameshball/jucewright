#pragma once

#include "../automation.h"
#include "native_services.h"

#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if JUCEWRIGHT_ENABLE_AUTOMATION && JUCE_MODULE_AVAILABLE_juce_opengl
    #include <juce_opengl/juce_opengl.h>
#endif

namespace jucewright
{
#if JUCEWRIGHT_ENABLE_AUTOMATION
    class AutomationController : private juce::Thread
    {
        #include "automation_controller_protocol.ipp"
        #include "automation_controller_queries.ipp"
        #include "automation_controller_screenshots.ipp"
        #include "automation_controller_pointer_actions.ipp"
        #include "automation_controller_value_actions.ipp"
        #include "automation_controller_locators.ipp"
        #include "automation_controller_input.ipp"
        #include "automation_controller_snapshots.ipp"
        #include "automation_controller_utilities.ipp"
    };
#endif
}
