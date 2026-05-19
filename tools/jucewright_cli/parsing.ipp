    bool isIntegerLiteral (juce::String text)
    {
        text = text.trim();

        if (text.isEmpty())
            return false;

        auto index = 0;

        if (text[0] == '-' || text[0] == '+')
            index = 1;

        if (index >= text.length())
            return false;

        for (; index < text.length(); ++index)
            if (! juce::CharacterFunctions::isDigit (text[index]))
                return false;

        return true;
    }

    bool isDecimalLiteral (juce::String text)
    {
        text = text.trim();

        if (text.isEmpty())
            return false;

        auto index = 0;
        auto sawDigit = false;
        auto sawDecimalPoint = false;

        if (text[0] == '-' || text[0] == '+')
            index = 1;

        for (; index < text.length(); ++index)
        {
            const auto c = text[index];

            if (juce::CharacterFunctions::isDigit (c))
            {
                sawDigit = true;
                continue;
            }

            if (c == '.' && ! sawDecimalPoint)
            {
                sawDecimalPoint = true;
                continue;
            }

            return false;
        }

        return sawDigit && sawDecimalPoint;
    }

    juce::var parseValue (const juce::String& text)
    {
        if (text == "true")
            return true;

        if (text == "false")
            return false;

        if (isIntegerLiteral (text))
            return text.getIntValue();

        if (isDecimalLiteral (text))
            return text.getDoubleValue();

        return text;
    }

    juce::var parseLocatorOptions (juce::StringArray& args)
    {
        auto locator = std::make_unique<juce::DynamicObject>();
        bool hasLocator = false;

        auto addString = [&] (const juce::String& option, const juce::String& property) {
            auto value = optionValue (args, option);

            if (value.isNotEmpty())
            {
                locator->setProperty (property, value);
                hasLocator = true;
            }
        };

        addString ("--role", "role");
        addString ("--name", "name");
        addString ("--text", "text");
        addString ("--component-id", "componentId");
        addString ("--component-name", "componentName");
        addString ("--test-id", "testId");
        addString ("--class", "class");
        addString ("--value", "value");
        addString ("--has-text", "hasText");

        auto nth = optionValue (args, "--nth");

        if (nth.isNotEmpty())
        {
            locator->setProperty ("nth", nth.getIntValue());
            hasLocator = true;
        }

        if (hasFlag (args, "--exact"))
        {
            locator->setProperty ("exact", true);
            hasLocator = true;
        }

        if (hasFlag (args, "--visible"))
        {
            locator->setProperty ("visible", true);
            hasLocator = true;
        }

        if (hasFlag (args, "--hidden"))
        {
            locator->setProperty ("visible", false);
            hasLocator = true;
        }

        if (hasFlag (args, "--enabled"))
        {
            locator->setProperty ("enabled", true);
            hasLocator = true;
        }

        if (hasFlag (args, "--disabled"))
        {
            locator->setProperty ("enabled", false);
            hasLocator = true;
        }

        if (hasFlag (args, "--focused"))
        {
            locator->setProperty ("focused", true);
            hasLocator = true;
        }

        if (hasFlag (args, "--selected"))
        {
            locator->setProperty ("selected", true);
            hasLocator = true;
        }

        if (hasFlag (args, "--not-selected"))
        {
            locator->setProperty ("selected", false);
            hasLocator = true;
        }

        if (hasFlag (args, "--accessible"))
        {
            locator->setProperty ("accessible", true);
            hasLocator = true;
        }

        if (hasFlag (args, "--inaccessible"))
        {
            locator->setProperty ("accessible", false);
            hasLocator = true;
        }

        return hasLocator ? juce::var (locator.release()) : juce::var();
    }

    juce::var parseTargetLocatorOptions (juce::StringArray& args)
    {
        auto locator = std::make_unique<juce::DynamicObject>();
        bool hasLocator = false;

        auto addString = [&] (const juce::String& flag, const juce::String& property)
        {
            auto value = optionValue (args, flag);

            if (value.isNotEmpty())
            {
                locator->setProperty (property, value);
                hasLocator = true;
            }
        };

        addString ("--target-role", "role");
        addString ("--target-name", "name");
        addString ("--target-text", "text");
        addString ("--target-component-id", "componentId");
        addString ("--target-component-name", "componentName");
        addString ("--target-test-id", "testId");
        addString ("--target-class", "class");
        addString ("--target-value", "value");

        auto nth = optionValue (args, "--target-nth");

        if (nth.isNotEmpty())
        {
            locator->setProperty ("nth", nth.getIntValue());
            hasLocator = true;
        }

        if (hasFlag (args, "--target-exact"))
        {
            locator->setProperty ("exact", true);
            hasLocator = true;
        }

        if (hasFlag (args, "--target-accessible"))
        {
            locator->setProperty ("accessible", true);
            hasLocator = true;
        }

        if (hasFlag (args, "--target-inaccessible"))
        {
            locator->setProperty ("accessible", false);
            hasLocator = true;
        }

        return hasLocator ? juce::var (locator.release()) : juce::var();
    }

    void addLocatorIfPresent (juce::DynamicObject& params, const juce::var& locator)
    {
        if (!locator.isVoid())
            params.setProperty ("locator", locator);
    }

    void addSnapshotOptions (juce::StringArray& args, juce::DynamicObject& params)
    {
        auto mode = optionValue (args, "--mode");

        if (hasFlag (args, "--full"))
            mode = "full";

        if (hasFlag (args, "--interesting"))
            mode = "interesting";

        if (hasFlag (args, "--minimal"))
            mode = "minimal";

        if (mode.isNotEmpty())
            params.setProperty ("mode", mode);

        auto ref = optionValue (args, "--ref");

        if (ref.isNotEmpty())
            params.setProperty ("ref", ref);

        auto target = optionValue (args, "--target");

        if (target.isNotEmpty())
            params.setProperty ("target", target);

        auto since = optionValue (args, "--since");

        if (since.isNotEmpty())
            params.setProperty ("since", since);

        auto maxNodes = optionValue (args, "--max-nodes");

        if (maxNodes.isNotEmpty())
            params.setProperty ("maxNodes", maxNodes.getIntValue());

        auto maxChildren = optionValue (args, "--max-children");

        if (maxChildren.isNotEmpty())
            params.setProperty ("maxChildrenPerContainer", maxChildren.getIntValue());

        auto maxText = optionValue (args, "--max-text");

        if (maxText.isNotEmpty())
            params.setProperty ("maxTextLength", maxText.getIntValue());

        if (hasFlag (args, "--include-hidden"))
            params.setProperty ("includeHidden", true);

        if (hasFlag (args, "--exclude-disabled"))
            params.setProperty ("includeDisabled", false);

        if (hasFlag (args, "--no-actions"))
            params.setProperty ("includeActions", false);

        if (hasFlag (args, "--no-bounds"))
            params.setProperty ("includeBounds", false);
    }

    void addTimeoutOption (juce::StringArray& args, juce::DynamicObject& params)
    {
        auto timeout = optionValue (args, "--timeout-ms", optionValue (args, "--timeout"));

        if (timeout.isNotEmpty())
            params.setProperty ("timeoutMs", timeout.getIntValue());
    }

    void addActionOptions (juce::StringArray& args, juce::DynamicObject& params)
    {
        addTimeoutOption (args, params);

        if (hasFlag (args, "--force"))
            params.setProperty ("force", true);

        if (hasFlag (args, "--trial"))
            params.setProperty ("trial", true);
    }

    void copyActionOptions (juce::DynamicObject& source, juce::DynamicObject& destination)
    {
        for (auto name : { "timeoutMs", "force", "trial" })
        {
            const auto value = source.getProperty (name);

            if (! value.isVoid())
                destination.setProperty (name, value);
        }
    }

    void addActionOptionsAndLocator (juce::DynamicObject& params, juce::DynamicObject& actionOptions, const juce::var& locator)
    {
        copyActionOptions (actionOptions, params);
        addLocatorIfPresent (params, locator);
    }

    juce::var object (std::initializer_list<std::pair<juce::String, juce::var>> properties)
    {
        auto* result = new juce::DynamicObject();

        for (const auto& property : properties)
            result->setProperty (property.first, property.second);

        return result;
    }

    juce::var parsePositionOption (const juce::String& position)
    {
        if (position.isEmpty())
            return {};

        juce::StringArray coordinates;
        coordinates.addTokens (position, ",", {});
        coordinates.trim();

        if (coordinates.size() != 2)
            throw std::runtime_error ("--position must use x,y");

        return object ({ { "x", coordinates[0].getDoubleValue() },
                         { "y", coordinates[1].getDoubleValue() } });
    }

    void addPositionIfPresent (juce::DynamicObject& params, const juce::String& position)
    {
        auto parsed = parsePositionOption (position);

        if (! parsed.isVoid())
            params.setProperty ("position", parsed);
    }

    juce::var emptyObject()
    {
        return juce::var (new juce::DynamicObject());
    }

    juce::var array (std::initializer_list<juce::var> values)
    {
        juce::Array<juce::var> result;

        for (const auto& value : values)
            result.add (value);

        return result;
    }
