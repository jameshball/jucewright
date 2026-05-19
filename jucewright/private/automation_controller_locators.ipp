        bool hasTargetSelector (juce::DynamicObject& params) const
        {
            return getString (params, "ref", {}).isNotEmpty() || params.getProperty ("locator").isObject();
        }

        TargetResolution resolveTarget (juce::DynamicObject& params, bool defaultVisible, bool requireStrict)
        {
            const auto ref = getString (params, "ref", {});
            auto locatorValue = params.getProperty ("locator");
            auto* locatorObject = locatorValue.getDynamicObject();

            if (ref.isNotEmpty() && locatorObject != nullptr)
                return { nullptr, error ("invalid_locator", "Pass either ref or locator, not both.") };

            if (ref.isNotEmpty())
            {
                auto* target = getTargetComponent (ref);
                return { target, target != nullptr ? juce::var() : errorWithSuggestion ("stale_ref", "Run snapshot again.", "snapshot") };
            }

            if (locatorObject == nullptr)
                return { nullptr, errorWithSuggestion ("stale_ref", "Run snapshot again.", "snapshot") };

            auto result = resolveLocatorQuery (*locatorObject, defaultVisible, requireStrict);

            if (isError (result))
                return { nullptr, result };

            auto* resultObject = result.getDynamicObject();
            auto matches = resultObject != nullptr ? resultObject->getProperty ("matches") : juce::var();

            if (!matches.isArray() || matches.getArray()->isEmpty())
                return { nullptr, errorWithSuggestion ("locator_not_found", "Locator did not match any component.", "snapshot") };

            auto refValue = asObjectProperty (matches.getArray()->getReference (0), "ref");
            auto* target = getTargetComponent (refValue);

            return { target, target != nullptr ? juce::var() : errorWithSuggestion ("stale_ref", "Run snapshot again.", "snapshot") };
        }

        juce::DynamicObject* getLocatorObject (juce::DynamicObject& params) const
        {
            auto locatorValue = params.getProperty ("locator");

            if (auto* locatorObject = locatorValue.getDynamicObject())
                return locatorObject;

            for (auto name : { "role",
                               "name",
                               "text",
                               "componentId",
                               "componentName",
                               "testId",
                               "class",
                               "value",
                               "hasText",
                               "nth",
                               "exact",
                               "visible",
                               "accessible",
                               "enabled",
                               "focused",
                               "selected" })
            {
                if (!params.getProperty (name).isVoid())
                    return &params;
            }

            return nullptr;
        }

        juce::var resolveLocatorQuery (juce::DynamicObject& params,
                                       bool defaultVisible,
                                       bool requireStrict,
                                       bool refreshRefs = true)
        {
            if (root == nullptr)
                return error ("no_root", "No root component is attached.");

            auto* locatorObject = getLocatorObject (params);

            if (locatorObject == nullptr)
                return error ("invalid_locator", "Locator must contain at least one field.");

            auto previousRefs = refs;
            const auto previousGeneration = generation;

            if (refreshRefs)
                pruneRefs();
            else
                refs.clear();

            if (refreshRefs)
                ++generation;

            auto tree = serializeAutomationTree (64);

            if (!refreshRefs)
            {
                refs = previousRefs;
                generation = previousGeneration;
            }

            juce::Array<juce::var> matches;
            collectLocatorMatches (matches, tree, *locatorObject, defaultVisible);

            const auto nthValue = locatorObject->getProperty ("nth");

            if (!nthValue.isVoid())
            {
                const auto nth = (int) nthValue;

                if (juce::isPositiveAndBelow (nth, matches.size()))
                {
                    auto selected = matches[nth];
                    matches.clear();
                    matches.add (selected);
                }
                else
                {
                    matches.clear();
                }
            }

            if (requireStrict)
            {
                if (matches.isEmpty())
                    return locatorError ("locator_not_found", "Locator did not match any component.", *locatorObject, matches);

                if (matches.size() > 1)
                    return locatorError ("strict_mode_violation",
                                         "Locator matched " + juce::String (matches.size()) + " components: " + summarizeMatches (matches),
                                         *locatorObject,
                                         matches);
            }

            return object ({ { "generation", generation },
                             { "stateHash", calculateStateHash (tree) },
                             { "count", matches.size() },
                             { "matches", matches } });
        }

        void collectLocatorMatches (juce::Array<juce::var>& matches, const juce::var& node, juce::DynamicObject& locatorObject, bool defaultVisible) const
        {
            auto* object = node.getDynamicObject();

            if (object == nullptr)
                return;

            if (matchesLocator (*object, locatorObject, defaultVisible))
                matches.add (summarizeNode (*object));

            auto children = object->getProperty ("children");

            if (children.isArray())
                for (auto& child : *children.getArray())
                    collectLocatorMatches (matches, child, locatorObject, defaultVisible);
        }

        static bool matchesLocator (juce::DynamicObject& node, juce::DynamicObject& locatorObject, bool defaultVisible)
        {
            const auto exact = (bool) locatorObject.getProperty ("exact");

            if (!locatorObject.getProperty ("accessible").isVoid())
            {
                if (!matchesOptionalBool (node, locatorObject, "accessible", "accessible")) return false;
            }
            else if (!isLocatorExposedNode (node))
            {
                return false;
            }

            if (!matchesOptionalString (node, locatorObject, "role", "role", exact)) return false;
            if (!matchesOptionalText (searchableName (node), locatorObject, "name", exact)) return false;
            if (!matchesOptionalText (searchableText (node), locatorObject, "text", exact)) return false;
            if (!matchesOptionalString (node, locatorObject, "componentId", "componentId", true)) return false;
            if (!matchesOptionalString (node, locatorObject, "componentId", "testId", true)) return false;
            if (!matchesOptionalString (node, locatorObject, "componentName", "componentName", true)) return false;
            if (!matchesOptionalString (node, locatorObject, "class", "class", exact)) return false;
            if (!matchesOptionalString (node, locatorObject, "value", "value", exact)) return false;
            if (!matchesOptionalText (searchableText (node), locatorObject, "hasText", exact)) return false;

            if (!matchesOptionalBool (node, locatorObject, "enabled", "enabled")) return false;
            if (!matchesOptionalBool (node, locatorObject, "focused", "focused")) return false;
            if (!matchesOptionalBool (node, locatorObject, "selected", "selected")) return false;

            if (!locatorObject.getProperty ("visible").isVoid())
            {
                if (!matchesOptionalBool (node, locatorObject, "visible", "visible"))
                    return false;
            }
            else if (defaultVisible && !(bool) node.getProperty ("visible"))
            {
                return false;
            }

            return true;
        }

        static bool matchesOptionalString (juce::DynamicObject& node,
                                           juce::DynamicObject& locatorObject,
                                           const juce::Identifier& nodeProperty,
                                           const juce::Identifier& locatorProperty,
                                           bool exact)
        {
            auto expected = locatorObject.getProperty (locatorProperty);

            if (expected.isVoid())
                return true;

            auto actual = node.getProperty (nodeProperty).toString();
            return matchesString (actual, expected.toString(), exact);
        }

        static bool matchesOptionalText (const juce::String& actual, juce::DynamicObject& locatorObject, const juce::Identifier& locatorProperty, bool exact)
        {
            auto expected = locatorObject.getProperty (locatorProperty);

            if (expected.isVoid())
                return true;

            return matchesString (actual, expected.toString(), exact);
        }

        static bool matchesOptionalBool (juce::DynamicObject& node,
                                         juce::DynamicObject& locatorObject,
                                         const juce::Identifier& nodeProperty,
                                         const juce::Identifier& locatorProperty)
        {
            auto expected = locatorObject.getProperty (locatorProperty);

            if (expected.isVoid())
                return true;

            return (bool) node.getProperty (nodeProperty) == (bool) expected;
        }

        static bool matchesString (const juce::String& actual, const juce::String& expected, bool exact)
        {
            const auto normalizedActual = normalizeForLocator (actual);
            const auto normalizedExpected = normalizeForLocator (expected);

            return exact ? normalizedActual == normalizedExpected
                         : normalizedActual.contains (normalizedExpected);
        }

        static juce::String normalizeForLocator (juce::String text)
        {
            return text.replaceCharacter ('\n', ' ')
                       .replaceCharacter ('\t', ' ')
                       .trim()
                       .replace ("  ", " ")
                       .toLowerCase();
        }

        static juce::String searchableName (juce::DynamicObject& node)
        {
            return node.getProperty ("title").toString().isNotEmpty()
                       ? node.getProperty ("title").toString()
                       : node.getProperty ("name").toString();
        }

        static juce::String searchableText (juce::DynamicObject& node)
        {
            return node.getProperty ("name").toString() + " "
                   + node.getProperty ("title").toString() + " "
                   + node.getProperty ("value").toString();
        }

        static bool treeContainsText (const juce::var& node, const juce::String& expected, bool exact, const juce::var& visible)
        {
            auto* object = node.getDynamicObject();

            if (object == nullptr)
                return false;

            const auto nodeVisible = (bool) object->getProperty ("visible");
            const auto visibilityMatches = visible.isVoid() ? nodeVisible
                                                            : nodeVisible == (bool) visible;

            if (visibilityMatches && matchesString (searchableText (*object), expected, exact))
                return true;

            auto children = object->getProperty ("children");

            if (children.isArray())
                for (const auto& child : *children.getArray())
                    if (treeContainsText (child, expected, exact, visible))
                        return true;

            return false;
        }

        static juce::String semanticValueFor (juce::Component& component)
        {
            if (auto* slider = dynamic_cast<juce::Slider*> (&component))
                return juce::String (slider->getValue());

            if (auto* combo = dynamic_cast<juce::ComboBox*> (&component))
                return combo->getText();

            if (auto* editor = dynamic_cast<juce::TextEditor*> (&component))
                return editor->getText();

            if (auto* label = dynamic_cast<juce::Label*> (&component))
                return label->getText();

            if (auto* button = dynamic_cast<juce::Button*> (&component))
                return button->getToggleState() ? "true" : "false";

            if (component.isAccessible() && component.getAccessibilityHandler() != nullptr)
                if (auto* valueInterface = component.getAccessibilityHandler()->getValueInterface())
                    return valueInterface->getCurrentValueAsString();

            return {};
        }

        static juce::var summarizeNode (juce::DynamicObject& node)
        {
            return object ({ { "ref", node.getProperty ("ref") },
                             { "name", node.getProperty ("name") },
                             { "componentId", node.getProperty ("componentId") },
                             { "componentName", node.getProperty ("componentName") },
                             { "class", node.getProperty ("class") },
                             { "role", node.getProperty ("role") },
                             { "value", node.getProperty ("value") },
                             { "visible", node.getProperty ("visible") },
                             { "enabled", node.getProperty ("enabled") },
                             { "focused", node.getProperty ("focused") },
                             { "bounds", node.getProperty ("bounds") } });
        }

        static juce::String summarizeMatches (const juce::Array<juce::var>& matches)
        {
            juce::StringArray lines;

            for (auto& match : matches)
            {
                if (auto* object = match.getDynamicObject())
                    lines.add (object->getProperty ("ref").toString()
                               + " " + object->getProperty ("class").toString()
                               + " \"" + object->getProperty ("name").toString() + "\"");
            }

            return lines.joinIntoString ("; ");
        }

        static bool isError (const juce::var& value)
        {
            if (auto* object = value.getDynamicObject())
                return object->getProperty ("__error").isString();

            return false;
        }

        void recordTraceEvent (const juce::String& method, juce::DynamicObject& params, const juce::var& result, double elapsedMs)
        {
            if (!traceEnabled || method == "trace_start" || method == "trace_stop")
                return;

            juce::String errorCode;
            juce::String errorMessage;

            if (auto* object = result.getDynamicObject())
            {
                errorCode = object->getProperty ("__error").toString();
                errorMessage = object->getProperty ("message").toString();
            }

            traceEvents.add (object ({ { "timeMs", (double) juce::Time::currentTimeMillis() },
                                       { "method", method },
                                       { "elapsedMs", elapsedMs },
                                       { "params", traceParamsSummary (params) },
                                       { "ok", errorCode.isEmpty() },
                                       { "error", errorCode },
                                       { "message", errorMessage },
                                       { "result", traceResultSummary (result) } }));
        }

        static juce::var traceParamsSummary (juce::DynamicObject& params)
        {
            auto* summary = new juce::DynamicObject();

            for (auto property : { "ref",
                                   "targetRef",
                                   "locator",
                                   "targetLocator",
                                   "description",
                                   "sourceRef",
                                   "sourceLocator",
                                   "x",
                                   "y",
                                   "toX",
                                   "toY",
                                   "dx",
                                   "dy",
                                   "steps",
                                   "position",
                                   "role",
                                   "name",
                                   "text",
                                   "menuItem",
                                   "componentId",
                                   "componentName",
                                   "value",
                                   "checked",
                                   "key",
                                   "file",
                                   "target",
                                   "source",
                                   "format",
                                   "mode",
                                   "depth",
                                   "since",
                                   "includeHidden",
                                   "includeDisabled",
                                   "includeActions",
                                   "includeBounds",
                                   "maxNodes",
                                   "maxChildrenPerContainer",
                                   "maxTextLength",
                                   "timeoutMs",
                                   "force",
                                   "trial",
                                   "visible",
                                   "exact",
                                   "stateHash" })
            {
                auto value = params.getProperty (property);

                if (!value.isVoid())
                    summary->setProperty (property, traceValueSummary (value));
            }

            return juce::var (summary);
        }

        static juce::var traceValueSummary (const juce::var& value)
        {
            if (value.isString())
            {
                auto text = value.toString();
                return text.length() > 512 ? text.substring (0, 512) + "..." : text;
            }

            return value;
        }

        static juce::var traceResultSummary (const juce::var& result)
        {
            auto* resultObject = result.getDynamicObject();

            if (resultObject == nullptr)
                return {};

            if (resultObject->getProperty ("__error").isString())
                return object ({ { "error", resultObject->getProperty ("__error") },
                                 { "message", resultObject->getProperty ("message") } });

            auto* summary = new juce::DynamicObject();

            for (auto property : { "stateHash",
                                   "generation",
                                   "count",
                                   "ref",
                                   "text",
                                   "value",
                                   "file",
                                   "trace",
                                   "events",
                                   "mimeType" })
            {
                auto value = resultObject->getProperty (property);

                if (!value.isVoid())
                    summary->setProperty (property, traceValueSummary (value));
            }

            if (auto actionability = resultObject->getProperty ("actionability"); actionability.isObject())
                summary->setProperty ("actionability", actionability);

            return juce::var (summary);
        }

        static juce::String asObjectProperty (const juce::var& value, const juce::Identifier& property)
        {
            if (auto* object = value.getDynamicObject())
                return object->getProperty (property).toString();

            return {};
        }

        juce::Component* requireTarget (juce::DynamicObject& params)
        {
            return getTargetComponent (getString (params, "ref", {}));
        }

        juce::Component* getTargetComponent (const juce::String& ref) const
        {
            if (ref.isEmpty())
                return nullptr;

            for (auto& entry : refs)
                if (entry.ref == ref)
                    return entry.component.getComponent();

            return nullptr;
        }

        void pruneRefs()
        {
            for (int i = refs.size(); --i >= 0;)
                if (refs.getReference (i).component.getComponent() == nullptr)
                    refs.remove (i);

            constexpr int maxRetainedRefs = 4096;

            if (refs.size() > maxRetainedRefs)
                refs.removeRange (0, refs.size() - maxRetainedRefs);
        }

        juce::ComponentPeer* getRootPeer() const
        {
            return root != nullptr ? root->getPeer() : nullptr;
        }
