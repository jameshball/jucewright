#include "../automation.h"

#if JUCEWRIGHT_ENABLE_AUTOMATION
    #include "automation_controller.h"
#endif

namespace jucewright
{
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
        options.sessionName = environmentVariableOrDefault ("JUCEWRIGHT_SESSION", defaultSessionName);

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
