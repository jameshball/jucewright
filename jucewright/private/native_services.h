#pragma once

#include "../automation.h"

namespace jucewright
{
#if JUCEWRIGHT_ENABLE_AUTOMATION
    struct NativeServices
    {
        static juce::File sessionsDirectory();
        static void restrictFilePermissions (const juce::File& file, int permissions);
        static int currentProcessId();
        static juce::Image createNativeScreenshot (juce::Component& target,
                                                   juce::Rectangle<int> area,
                                                   float scale,
                                                   juce::String& failure);
    };
#endif
}
