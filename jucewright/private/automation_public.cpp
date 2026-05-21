#include "../automation.h"

#if JUCEWRIGHT_ENABLE_AUTOMATION
    #include "automation_controller.h"
#endif

namespace jucewright
{
    juce::String defaultAutomationSessionName()
    {
#if defined (JucePlugin_Name)
        auto name = juce::String (JucePlugin_Name);

        if (name.isNotEmpty())
            return name;
#endif

        if (auto* app = juce::JUCEApplicationBase::getInstance())
            return app->getApplicationName();

        return "jucewright";
    }

    Automation::Automation() = default;

    Automation::~Automation()
    {
        disable();
    }

    void Automation::enable (juce::Component& rootComponent, AutomationOptions options)
    {
#if JUCEWRIGHT_ENABLE_AUTOMATION
        controller = std::make_unique<AutomationController> (rootComponent, std::move (options));
#else
        juce::ignoreUnused (rootComponent, options);
#endif
    }

    bool Automation::enableFromEnvironment (juce::Component& rootComponent,
                                            const juce::String& defaultSessionName,
                                            const juce::String& extraTriggerEnvironmentVariable)
    {
#if JUCEWRIGHT_ENABLE_AUTOMATION
        const auto trigger = environmentVariableOrDefault ("JUCEWRIGHT_AUTOMATION");
        const auto extraTrigger = extraTriggerEnvironmentVariable.isNotEmpty()
                                      ? juce::SystemStats::getEnvironmentVariable (extraTriggerEnvironmentVariable, {})
                                      : juce::String();

        if (trigger.isEmpty() && extraTrigger.isEmpty())
            return false;

        AutomationOptions options;
        const auto fallbackSessionName = defaultSessionName.isNotEmpty() ? defaultSessionName : defaultAutomationSessionName();
        options.sessionName = environmentVariableOrDefault ("JUCEWRIGHT_SESSION", fallbackSessionName);

        const auto artifactRootPath = environmentVariableOrDefault ("JUCEWRIGHT_ARTIFACT_ROOT");
        options.allowFileWrite = true;
        options.artifactRoot = artifactRootPath.isNotEmpty()
                                   ? juce::File::createFileWithoutCheckingPath (artifactRootPath)
                                   : juce::File::getSpecialLocation (juce::File::tempDirectory)
                                         .getChildFile (options.sessionName + "-jucewright");
        options.artifactRoot.createDirectory();

        enable (rootComponent, std::move (options));

        if (! isRunning())
            juce::Logger::writeToLog ("Jucewright automation was requested but the automation server did not start");

        return isRunning();
#else
        juce::ignoreUnused (rootComponent, defaultSessionName, extraTriggerEnvironmentVariable);
        return false;
#endif
    }

    struct EnvironmentAutomation::State
    {
        State (juce::Component& rootComponent,
               juce::String defaultSessionNameToUse,
               juce::String extraTriggerEnvironmentVariableToUse)
            : root (&rootComponent),
              defaultSessionName (std::move (defaultSessionNameToUse)),
              extraTriggerEnvironmentVariable (std::move (extraTriggerEnvironmentVariableToUse))
        {
        }

        juce::Component::SafePointer<juce::Component> root;
        juce::String defaultSessionName;
        juce::String extraTriggerEnvironmentVariable;
        Automation automation;
        bool cancelled = false;
    };

    EnvironmentAutomation::EnvironmentAutomation (juce::Component& rootComponent,
                                                  juce::String defaultSessionName,
                                                  juce::String extraTriggerEnvironmentVariable)
        : state (std::make_shared<State> (rootComponent,
                                          std::move (defaultSessionName),
                                          std::move (extraTriggerEnvironmentVariable)))
    {
        auto sharedState = state;

        juce::MessageManager::callAsync ([sharedState] {
            auto* rootComponent = sharedState->root.getComponent();

            if (sharedState->cancelled || rootComponent == nullptr)
                return;

            sharedState->automation.enableFromEnvironment (*rootComponent,
                                                           sharedState->defaultSessionName,
                                                           sharedState->extraTriggerEnvironmentVariable);
        });
    }

    EnvironmentAutomation::~EnvironmentAutomation()
    {
        if (state == nullptr)
            return;

        state->cancelled = true;
        state->automation.disable();
        state.reset();
    }

    void Automation::updateRoot (juce::Component& rootComponent)
    {
#if JUCEWRIGHT_ENABLE_AUTOMATION
        if (controller != nullptr)
            controller->updateRoot (rootComponent);
#else
        juce::ignoreUnused (rootComponent);
#endif
    }

    void Automation::clearRoot()
    {
#if JUCEWRIGHT_ENABLE_AUTOMATION
        if (controller != nullptr)
            controller->clearRoot();
#endif
    }

    void Automation::disable()
    {
#if JUCEWRIGHT_ENABLE_AUTOMATION
        controller.reset();
#endif
    }

    bool Automation::isRunning() const
    {
#if JUCEWRIGHT_ENABLE_AUTOMATION
        return controller != nullptr && controller->isRunning();
#else
        return false;
#endif
    }
}
