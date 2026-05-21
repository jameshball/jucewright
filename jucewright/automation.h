#pragma once

#include "component_naming.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <utility>

#ifndef JUCEWRIGHT_ENABLE_AUTOMATION
    #if defined (JUCE_DEBUG) && JUCE_DEBUG
        #define JUCEWRIGHT_ENABLE_AUTOMATION 1
    #elif defined (DEBUG) || defined (_DEBUG)
        #define JUCEWRIGHT_ENABLE_AUTOMATION 1
    #else
        #define JUCEWRIGHT_ENABLE_AUTOMATION 0
    #endif
#endif

namespace jucewright
{
    struct AutomationOptions
    {
        juce::String sessionName;
        juce::String authToken;
        int port = 0;
        bool advertise = true;
        bool allowInput = true;
        bool allowMutation = true;
        bool allowFileWrite = false;
        juce::File artifactRoot;
    };

#if JUCEWRIGHT_ENABLE_AUTOMATION
    class AutomationController;
#endif

    inline juce::String environmentVariableOrDefault (const juce::String& name, const juce::String& defaultValue = {})
    {
        auto value = juce::SystemStats::getEnvironmentVariable (name, {});
        return value.isEmpty() ? defaultValue : value;
    }

    [[nodiscard]] juce::String defaultAutomationSessionName();

    class Automation
    {
    public:
        Automation();
        ~Automation();

        void enable (juce::Component& rootComponent, AutomationOptions options = {});
        bool enableFromEnvironment (juce::Component& rootComponent,
                                    const juce::String& defaultSessionName = {},
                                    const juce::String& extraTriggerEnvironmentVariable = {});
        void updateRoot (juce::Component& rootComponent);
        void clearRoot();
        void disable();

        [[nodiscard]] bool isRunning() const;

    private:
#if JUCEWRIGHT_ENABLE_AUTOMATION
        std::unique_ptr<AutomationController> controller;
#endif
    };

    class EnvironmentAutomation
    {
    public:
        explicit EnvironmentAutomation (juce::Component& rootComponent,
                                        juce::String defaultSessionName = {},
                                        juce::String extraTriggerEnvironmentVariable = {});
        ~EnvironmentAutomation();

        EnvironmentAutomation (const EnvironmentAutomation&) = delete;
        EnvironmentAutomation& operator= (const EnvironmentAutomation&) = delete;

    private:
        struct State;
        std::shared_ptr<State> state;
    };
}
