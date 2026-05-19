#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <typeinfo>

#if JUCE_MODULE_AVAILABLE_juce_audio_processors
    #include <juce_audio_processors/juce_audio_processors.h>
#endif

#if ! defined (_MSC_VER)
    #include <cxxabi.h>
#endif

namespace jucewright
{
#if ! defined (_MSC_VER)
    static inline std::string demangle (const char* name)
    {
        int status = -4;

        std::unique_ptr<char, void (*) (void*)> res {
            abi::__cxa_demangle (name, nullptr, nullptr, &status),
            std::free
        };

        return status == 0 ? res.get() : name;
    }

    template <class T>
    static inline juce::String type (const T& value)
    {
        return demangle (typeid (value).name());
    }
#else
    template <class T>
    static inline juce::String type (const T& value)
    {
        return juce::String (typeid (value).name()).replace ("class ", "").replace ("struct ", "");
    }
#endif

    static inline juce::String trimComponentSuffix (juce::String name)
    {
        if (name.endsWith ("Component"))
            name = name.dropLastCharacters (9);

        if (name.endsWith ("Button"))
            name = name.dropLastCharacters (6) + " button";

        return name;
    }

    static inline juce::String humanizeIdentifier (juce::String name)
    {
        if (name.contains ("::"))
            name = name.fromLastOccurrenceOf ("::", false, false);

        if (name.contains ("<"))
            name = name.upToFirstOccurrenceOf ("<", false, false);

        name = trimComponentSuffix (name);

        juce::String result;
        juce::juce_wchar previous = 0;

        for (auto c : name)
        {
            if ((c == '_' || c == '-') && result.isNotEmpty())
            {
                result << " ";
                previous = ' ';
                continue;
            }

            if (previous != 0 && previous != ' ' && juce::CharacterFunctions::isUpperCase (c)
                && (juce::CharacterFunctions::isLowerCase (previous) || juce::CharacterFunctions::isDigit (previous)))
                result << " ";

            result << c;
            previous = c;
        }

        return result.trim();
    }

    static inline juce::String limitedComponentText (const juce::String& text)
    {
        auto trimmed = text.trim();
        return trimmed.length() > 80 ? trimmed.substring (0, 80) + "..." : trimmed;
    }

    static inline juce::String textForLabellingComponent (juce::Component& component)
    {
        if (auto* label = dynamic_cast<juce::Label*> (&component))
            return limitedComponentText (label->getText());

        if (auto* group = dynamic_cast<juce::GroupComponent*> (&component))
            return limitedComponentText (group->getText());

        if (auto* button = dynamic_cast<juce::Button*> (&component))
            return limitedComponentText (button->getButtonText());

        if (auto* tooltip = dynamic_cast<juce::TooltipClient*> (&component))
            return limitedComponentText (tooltip->getTooltip());

        return {};
    }

    static inline juce::String textForNearbyLabellingComponent (juce::Component& component)
    {
        if (auto* label = dynamic_cast<juce::Label*> (&component))
            return limitedComponentText (label->getText());

        if (auto* group = dynamic_cast<juce::GroupComponent*> (&component))
            return limitedComponentText (group->getText());

        return {};
    }

    static inline juce::String normalizedComponentText (juce::String text)
    {
        text = text.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789").toLowerCase();
        return text.trim();
    }

    static inline bool isGenericComponentText (juce::Component& component, const juce::String& text)
    {
        const auto normalized = normalizedComponentText (text);

        if (normalized.isEmpty())
            return true;

        const auto className = type (component);
        auto leafClassName = className.fromLastOccurrenceOf ("::", false, false);

        if (leafClassName.contains ("<"))
            leafClassName = leafClassName.upToFirstOccurrenceOf ("<", false, false);

        juce::StringArray genericNames;
        genericNames.add (className);
        genericNames.add (leafClassName);
        genericNames.add (humanizeIdentifier (className));
        genericNames.add (humanizeIdentifier (leafClassName));
        genericNames.add (trimComponentSuffix (leafClassName));

        for (const auto& genericName : genericNames)
            if (normalized == normalizedComponentText (genericName))
                return true;

        return normalized == "component" || normalized == "button" || normalized == "switchbutton";
    }

    static inline juce::String explicitOwnComponentText (juce::Component& component)
    {
        if (component.isAccessible() && component.getAccessibilityHandler() != nullptr)
        {
            auto title = limitedComponentText (component.getAccessibilityHandler()->getTitle());

            if (title.isNotEmpty() && ! isGenericComponentText (component, title))
                return title;
        }

        auto name = limitedComponentText (component.getName());

        if (name.isNotEmpty() && ! isGenericComponentText (component, name))
            return name;

        return {};
    }

    static inline juce::String parentCompositeName (juce::Component& component)
    {
        auto* parent = component.getParentComponent();

        if (parent == nullptr)
            return {};

        if (auto text = explicitOwnComponentText (*parent); text.isNotEmpty())
            return text;

        return {};
    }

    static inline int overlapOnAxis (int startA, int endA, int startB, int endB)
    {
        return juce::jmax (0, juce::jmin (endA, endB) - juce::jmax (startA, startB));
    }

    static inline juce::String nearbyLabelText (juce::Component& target)
    {
        auto* parent = target.getParentComponent();

        if (parent == nullptr)
            return {};

        const auto targetBounds = target.getBounds();
        juce::String best;
        auto bestScore = std::numeric_limits<int>::max();

        for (int i = 0; i < parent->getNumChildComponents(); ++i)
        {
            auto* sibling = parent->getChildComponent (i);

            if (sibling == nullptr || sibling == &target || ! sibling->isShowing())
                continue;

            const auto text = textForNearbyLabellingComponent (*sibling);

            if (text.isEmpty())
                continue;

            const auto bounds = sibling->getBounds();
            const auto verticalOverlap = overlapOnAxis (bounds.getY(), bounds.getBottom(), targetBounds.getY(), targetBounds.getBottom());
            const auto horizontalOverlap = overlapOnAxis (bounds.getX(), bounds.getRight(), targetBounds.getX(), targetBounds.getRight());
            auto score = std::numeric_limits<int>::max();

            if (verticalOverlap > 0 && bounds.getRight() <= targetBounds.getX())
                score = targetBounds.getX() - bounds.getRight();
            else if (horizontalOverlap > 0 && bounds.getBottom() <= targetBounds.getY())
                score = targetBounds.getY() - bounds.getBottom() + 1000;

            if (score >= 0 && score < bestScore && score < 1200)
            {
                best = text;
                bestScore = score;
            }
        }

        return best;
    }

    static inline juce::String componentString (juce::Component* component)
    {
        if (component == nullptr)
            return {};

       #if JUCE_MODULE_AVAILABLE_juce_audio_processors
        if (auto* editor = dynamic_cast<juce::AudioProcessorEditor*> (component))
            return juce::String ("Editor: ") + editor->getAudioProcessor()->getName();
       #endif

        if (auto text = explicitOwnComponentText (*component); text.isNotEmpty())
            return text;

        if (auto* button = dynamic_cast<juce::Button*> (component))
            if (auto text = limitedComponentText (button->getButtonText()); text.isNotEmpty())
                return text;

        if (auto* label = dynamic_cast<juce::Label*> (component))
            if (auto text = limitedComponentText (label->getText()); text.isNotEmpty())
                return text;

        if (auto* group = dynamic_cast<juce::GroupComponent*> (component))
            if (auto text = limitedComponentText (group->getText()); text.isNotEmpty())
                return text;

        if (auto* tooltip = dynamic_cast<juce::TooltipClient*> (component))
            if (auto text = limitedComponentText (tooltip->getTooltip()); text.isNotEmpty())
                return text;

        if (auto text = parentCompositeName (*component); text.isNotEmpty())
            return text;

        if (auto text = nearbyLabelText (*component); text.isNotEmpty())
            return text;

        return humanizeIdentifier (type (*component));
    }
}
