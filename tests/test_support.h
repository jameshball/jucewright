#pragma once

#include <juce_core/juce_core.h>
#include <initializer_list>
#include <stdexcept>

namespace jucewright_test
{
    inline juce::File tempDirectory()
    {
        const auto fallback = juce::File::getSpecialLocation (juce::File::tempDirectory).getFullPathName();
        const auto temp = juce::SystemStats::getEnvironmentVariable (
        #if JUCE_WINDOWS
            "TEMP",
        #else
            "TMPDIR",
        #endif
            fallback);

        return juce::File (temp);
    }

    inline juce::File sessionsDirectory()
    {
        return tempDirectory().getChildFile ("jucewright").getChildFile ("sessions");
    }

    [[noreturn]] inline void fail (const juce::String& message)
    {
        throw std::runtime_error (message.toStdString());
    }

    inline void require (bool condition, const juce::String& message)
    {
        if (!condition)
            fail (message);
    }

    inline juce::StringArray makeArgs (std::initializer_list<juce::String> values)
    {
        juce::StringArray result;

        for (const auto& value : values)
            result.add (value);

        return result;
    }

    inline juce::String shellQuote (juce::String value)
    {
    #if JUCE_WINDOWS
        return "\"" + value.replace ("\"", "\\\"") + "\"";
    #else
        return "'" + value.replace ("'", "'\"'\"'") + "'";
    #endif
    }

    inline juce::DynamicObject& asObject (const juce::var& value, const juce::String& context)
    {
        auto* object = value.getDynamicObject();
        require (object != nullptr, context + " is not an object: " + juce::JSON::toString (value, true));
        return *object;
    }

    inline juce::var parseJsonOutput (const juce::String& output, const juce::String& context)
    {
        auto parsed = juce::JSON::parse (output.trim());

        if (parsed.isObject() || parsed.isArray())
            return parsed;

        auto objectStart = output.indexOfChar ('{');
        auto arrayStart = output.indexOfChar ('[');
        auto start = objectStart < 0 ? arrayStart : (arrayStart < 0 ? objectStart : juce::jmin (objectStart, arrayStart));

        if (start >= 0)
        {
            auto depth = 0;
            auto inString = false;
            auto escaped = false;

            for (int i = start; i < output.length(); ++i)
            {
                const auto c = output[i];

                if (inString)
                {
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (c == '\\')
                    {
                        escaped = true;
                    }
                    else if (c == '"')
                    {
                        inString = false;
                    }

                    continue;
                }

                if (c == '"')
                {
                    inString = true;
                    continue;
                }

                if (c == '{' || c == '[')
                    ++depth;
                else if (c == '}' || c == ']')
                    --depth;

                if (depth == 0)
                {
                    parsed = juce::JSON::parse (output.substring (start, i + 1));

                    if (parsed.isObject() || parsed.isArray())
                        return parsed;

                    break;
                }
            }
        }

        for (auto line : juce::StringArray::fromLines (output))
        {
            auto trimmed = line.trim();

            if (trimmed.startsWithChar ('{') || trimmed.startsWithChar ('['))
            {
                parsed = juce::JSON::parse (trimmed);

                if (parsed.isObject() || parsed.isArray())
                    return parsed;
            }
        }

        fail (context + " did not contain JSON\n" + output);
    }
}
