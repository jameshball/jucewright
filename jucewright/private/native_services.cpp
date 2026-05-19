#include "native_services.h"

#if JUCEWRIGHT_ENABLE_AUTOMATION
    #include <cstdlib>

    #if JUCE_WINDOWS
        #include <windows.h>
    #else
        #include <sys/stat.h>
        #include <unistd.h>
    #endif

namespace jucewright
{
    juce::File NativeServices::sessionsDirectory()
    {
        const char* temp = std::getenv (
#if JUCE_WINDOWS
            "TEMP"
#else
            "TMPDIR"
#endif
        );

        auto directory = juce::File (temp != nullptr ? juce::String::fromUTF8 (temp) : juce::String ("/tmp"))
                             .getChildFile ("jucewright")
                             .getChildFile ("sessions");
        directory.createDirectory();
        restrictFilePermissions (directory, 0700);
        return directory;
    }

    void NativeServices::restrictFilePermissions (const juce::File& file, int permissions)
    {
#if JUCE_WINDOWS
        juce::ignoreUnused (file, permissions);
#else
        ::chmod (file.getFullPathName().toRawUTF8(), (mode_t) permissions);
#endif
    }

    int NativeServices::currentProcessId()
    {
#if JUCE_WINDOWS
        return (int) ::GetCurrentProcessId();
#else
        return (int) ::getpid();
#endif
    }

    juce::Image NativeServices::createNativeScreenshot (juce::Component& target,
                                                        juce::Rectangle<int> area,
                                                        float scale,
                                                        juce::String& failure)
    {
#if JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX || JUCE_BSD
        auto* topLevel = target.getTopLevelComponent();

        if (topLevel == nullptr)
            topLevel = &target;

        auto* nativeHandle = topLevel->getWindowHandle();

        if (nativeHandle == nullptr)
        {
            failure = "Target top-level component does not have a native window handle.";
            return {};
        }

        auto nativeImage = juce::createSnapshotOfNativeWindow (nativeHandle);

        if (nativeImage.isNull())
        {
            failure = "JUCE could not capture the native window.";
            return {};
        }

        auto crop = topLevel->getLocalArea (&target, area);

        if (auto* peer = topLevel->getPeer())
        {
            auto windowFrame = peer->getBounds();

            if (const auto frameSize = peer->getFrameSizeIfPresent())
                windowFrame = frameSize->addedTo (windowFrame);

            if (! windowFrame.isEmpty())
            {
                const auto globalArea = target.localAreaToGlobal (area);
                const auto scaleX = (double) nativeImage.getWidth() / (double) windowFrame.getWidth();
                const auto scaleY = (double) nativeImage.getHeight() / (double) windowFrame.getHeight();

                crop = juce::Rectangle<int> {
                    juce::roundToInt ((double) (globalArea.getX() - windowFrame.getX()) * scaleX),
                    juce::roundToInt ((double) (globalArea.getY() - windowFrame.getY()) * scaleY),
                    juce::roundToInt ((double) globalArea.getWidth() * scaleX),
                    juce::roundToInt ((double) globalArea.getHeight() * scaleY)
                };
            }
        }

        crop = crop.getIntersection (nativeImage.getBounds());

        if (crop.isEmpty())
        {
            failure = "Native screenshot crop is outside the captured window.";
            return {};
        }

        auto image = nativeImage.getClippedImage (crop);

        if (scale > 0.0f && scale != 1.0f)
            image = image.rescaled (juce::roundToInt ((float) image.getWidth() * scale),
                                    juce::roundToInt ((float) image.getHeight() * scale));

        return image;
#else
        juce::ignoreUnused (target, area, scale);
        failure = "Native screenshots are not supported on this platform.";
        return {};
#endif
    }
}
#endif
