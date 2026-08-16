        static void appendTextSnapshot (juce::String& out, const juce::var& node, int indent)
        {
            auto* object = node.getDynamicObject();

            if (object == nullptr)
                return;

            auto bounds = object->getProperty ("bounds").getDynamicObject();
            auto box = bounds != nullptr
                           ? bounds->getProperty ("x").toString() + "," + bounds->getProperty ("y").toString() + "," + bounds->getProperty ("w").toString() + "," + bounds->getProperty ("h").toString()
                           : "-";

            out << juce::String::repeatedString ("  ", indent)
                << "- " << object->getProperty ("class").toString()
                << " \"" << object->getProperty ("name").toString() << "\""
                << " [ref=" << object->getProperty ("ref").toString()
                << " box=" << box;

            auto componentId = object->getProperty ("componentId").toString();

            if (componentId.isNotEmpty())
                out << " id=" << componentId;

            auto componentName = object->getProperty ("componentName").toString();

            if (componentName.isNotEmpty() && componentName != object->getProperty ("name").toString())
                out << " componentName=" << componentName;

            auto role = object->getProperty ("role").toString();

            if (role.isNotEmpty())
                out << " role=" << role;

            auto value = object->getProperty ("value").toString();

            if (value.isNotEmpty())
                out << " value=\"" << value << "\"";

            if (!(bool) object->getProperty ("visible"))
                out << " hidden=true";

            if (!(bool) object->getProperty ("enabled"))
                out << " disabled=true";

            if ((bool) object->getProperty ("selected"))
                out << " selected=true";

            if ((bool) object->getProperty ("checked"))
                out << " checked=true";

            auto currentTab = object->getProperty ("currentTab").toString();

            if (currentTab.isNotEmpty())
                out << " currentTab=\"" << currentTab << "\"";

            auto actions = object->getProperty ("actions");

            if (actions.isArray())
            {
                juce::StringArray actionNames;

                for (auto& action : *actions.getArray())
                    actionNames.add (action.toString());

                if (!actionNames.isEmpty())
                    out << " actions=" << actionNames.joinIntoString (",");
            }

            auto omittedChildren = object->getProperty ("omittedChildren");

            if (!omittedChildren.isVoid() && (int) omittedChildren > 0)
                out << " omittedChildren=" << omittedChildren.toString();

            auto omittedOptions = object->getProperty ("omittedOptions");

            if (!omittedOptions.isVoid() && (int) omittedOptions > 0)
                out << " omittedOptions=" << omittedOptions.toString();

            out << "]\n";

            auto children = object->getProperty ("children");

            if (children.isArray())
                for (auto& child : *children.getArray())
                    appendTextSnapshot (out, child, indent + 1);
        }

        static juce::String calculateStateHash (const juce::var& tree)
        {
            const auto canonical = canonicalizeSnapshotNode (tree);
            return juce::String::toHexString (juce::JSON::toString (canonical, true).hashCode64());
        }

        static juce::var canonicalizeSnapshotNode (const juce::var& node)
        {
            auto* object = node.getDynamicObject();

            if (object == nullptr)
                return {};

            auto* result = new juce::DynamicObject();

            for (auto name : { "name",
                               "componentId",
                               "componentName",
                               "class",
                               "enabled",
                               "visible",
                               "accessible",
                               "focused",
                               "role",
                               "title",
                               "value",
                               "selectable",
                               "selected",
                               "expandable",
                               "expanded",
                               "collapsed",
                               "toggleable",
                               "toggleState",
                               "checked",
                               "editable",
                               "readOnly",
                               "selectedIndex",
                               "selectedId",
                               "selectedText",
                               "minimum",
                               "maximum",
                               "interval",
                               "tabNames",
                               "currentTabIndex",
                               "currentTab",
                               "scrollX",
                               "scrollY",
                               "viewWidth",
                               "viewHeight",
                               "contentWidth",
                               "contentHeight",
                               "rowCount",
                               "selectedRow",
                               "selectedRows",
                               "options",
                               "documentCount",
                               "layoutMode",
                               "activeDocument" })
            {
                auto property = object->getProperty (name);

                if (!property.isVoid())
                    result->setProperty (name, property);
            }

            result->setProperty ("bounds", object->getProperty ("bounds"));

            juce::Array<juce::var> children;
            auto sourceChildren = object->getProperty ("children");

            if (sourceChildren.isArray())
                for (auto& child : *sourceChildren.getArray())
                    children.add (canonicalizeSnapshotNode (child));

            result->setProperty ("children", children);
            return result;
        }

        static juce::var rectangleToVar (juce::Rectangle<int> rectangle)
        {
            return object ({ { "x", rectangle.getX() },
                             { "y", rectangle.getY() },
                             { "w", rectangle.getWidth() },
                             { "h", rectangle.getHeight() } });
        }

        static juce::String accessibilityRoleName (juce::AccessibilityRole role)
        {
            switch (role)
            {
                case juce::AccessibilityRole::button: return "button";
                case juce::AccessibilityRole::toggleButton: return "toggleButton";
                case juce::AccessibilityRole::radioButton: return "radioButton";
                case juce::AccessibilityRole::comboBox: return "comboBox";
                case juce::AccessibilityRole::image: return "image";
                case juce::AccessibilityRole::slider: return "slider";
                case juce::AccessibilityRole::label: return "label";
                case juce::AccessibilityRole::staticText: return "staticText";
                case juce::AccessibilityRole::editableText: return "editableText";
                case juce::AccessibilityRole::menuItem: return "menuItem";
                case juce::AccessibilityRole::menuBar: return "menuBar";
                case juce::AccessibilityRole::popupMenu: return "popupMenu";
                case juce::AccessibilityRole::table: return "table";
                case juce::AccessibilityRole::tableHeader: return "tableHeader";
                case juce::AccessibilityRole::column: return "column";
                case juce::AccessibilityRole::row: return "row";
                case juce::AccessibilityRole::cell: return "cell";
                case juce::AccessibilityRole::hyperlink: return "hyperlink";
                case juce::AccessibilityRole::list: return "list";
                case juce::AccessibilityRole::listItem: return "listItem";
                case juce::AccessibilityRole::tree: return "tree";
                case juce::AccessibilityRole::treeItem: return "treeItem";
                case juce::AccessibilityRole::progressBar: return "progressBar";
                case juce::AccessibilityRole::group: return "group";
                case juce::AccessibilityRole::dialogWindow: return "dialogWindow";
                case juce::AccessibilityRole::window: return "window";
                case juce::AccessibilityRole::scrollBar: return "scrollBar";
                case juce::AccessibilityRole::tooltip: return "tooltip";
                case juce::AccessibilityRole::splashScreen: return "splashScreen";
                case juce::AccessibilityRole::ignored: return "ignored";
                case juce::AccessibilityRole::unspecified: return "unspecified";
                default: break;
            }

            return "unknown";
        }

        struct ParsedKey
        {
            juce::KeyPress keyPress;
        };

        static ParsedKey parseKey (const juce::String& key)
        {
            juce::StringArray tokens;
            tokens.addTokens (key, "+", {});
            tokens.trim();
            tokens.removeEmptyStrings();

            juce::ModifierKeys modifiers;
            auto keyName = tokens.isEmpty() ? key : tokens[tokens.size() - 1];

            for (int i = 0; i < tokens.size() - 1; ++i)
            {
                auto modifier = tokens[i].trim().toLowerCase();

                if (modifier == "shift")
                    modifiers = modifiers.withFlags (juce::ModifierKeys::shiftModifier);
                else if (modifier == "control" || modifier == "ctrl")
                    modifiers = modifiers.withFlags (juce::ModifierKeys::ctrlModifier);
                else if (modifier == "alt" || modifier == "option")
                    modifiers = modifiers.withFlags (juce::ModifierKeys::altModifier);
                else if (modifier == "meta" || modifier == "cmd" || modifier == "command")
                    modifiers = modifiers.withFlags (juce::ModifierKeys::commandModifier);
            }

            const auto keyCode = keyCodeForName (keyName);
            const auto textCharacter = keyName.length() == 1 ? keyName[0] : juce::juce_wchar();
            return { juce::KeyPress (keyCode, modifiers, textCharacter) };
        }

        static int keyCodeForName (juce::String key)
        {
            key = key.trim().toLowerCase();

            if (key == "tab") return juce::KeyPress::tabKey;
            if (key == "return" || key == "enter") return juce::KeyPress::returnKey;
            if (key == "escape" || key == "esc") return juce::KeyPress::escapeKey;
            if (key == "backspace") return juce::KeyPress::backspaceKey;
            if (key == "delete") return juce::KeyPress::deleteKey;
            if (key == "left") return juce::KeyPress::leftKey;
            if (key == "right") return juce::KeyPress::rightKey;
            if (key == "up") return juce::KeyPress::upKey;
            if (key == "down") return juce::KeyPress::downKey;

            if (key.startsWithChar ('f'))
            {
                const auto functionKey = key.substring (1).getIntValue();

                if (functionKey >= 1 && functionKey <= 35 && key == "f" + juce::String (functionKey))
                    return juce::KeyPress::F1Key + functionKey - 1;
            }

            if (key.length() == 1) return juce::CharacterFunctions::toUpperCase (key[0]);

            return 0;
        }

        juce::var callOnMessageThread (std::function<juce::var()> function)
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                return function();

            struct Call
            {
                explicit Call (std::function<juce::var()> fn)
                    : function (std::move (fn))
                {
                }

                std::function<juce::var()> function;
                juce::var result;
                juce::WaitableEvent completed;
                std::atomic<bool> cancelled { false };
            };

            auto call = std::make_shared<Call> (std::move (function));

            if (!juce::MessageManager::callAsync ([call] {
                    if (!call->cancelled.load())
                        call->result = call->function();

                    call->completed.signal();
                }))
            {
                return error ("message_thread_unavailable", "Could not post automation request to the JUCE message thread.");
            }

            while (!threadShouldExit())
                if (call->completed.wait (25))
                    return call->result;

            call->cancelled = true;
            return error ("shutting_down", "Automation endpoint is shutting down.");
        }

        void sleepUntilReadyOrStopped (int milliseconds)
        {
            auto remaining = milliseconds;

            while (remaining > 0 && !threadShouldExit())
            {
                const auto chunk = juce::jmin (remaining, 25);
                juce::Thread::sleep (chunk);
                remaining -= chunk;
            }
        }

        void waitForThreadToStop()
        {
            waitForThreadToExit (-1);
        }

        static juce::var object (std::initializer_list<std::pair<juce::String, juce::var>> properties)
        {
            auto* result = new juce::DynamicObject();

            for (const auto& property : properties)
                result->setProperty (property.first, property.second);

            return result;
        }

        static juce::var error (const juce::String& code, const juce::String& message)
        {
            return object ({ { "__error", code }, { "message", message } });
        }

        static juce::var errorWithSuggestion (const juce::String& code,
                                              const juce::String& message,
                                              const juce::String& suggestedNextCommand)
        {
            return object ({ { "__error", code },
                             { "message", message },
                             { "suggestedNextCommand", suggestedNextCommand } });
        }

        static juce::String responseOk (const juce::String& id, const juce::var& result)
        {
            return juce::JSON::toString (object ({ { "id", id }, { "ok", true }, { "result", result } }), true);
        }

        static juce::String responseError (const juce::String& id, const juce::String& code, const juce::String& message)
        {
            return juce::JSON::toString (object ({ { "id", id },
                                                   { "ok", false },
                                                   { "error", object ({ { "code", code }, { "message", message } }) } }),
                                         true);
        }

        static juce::String responseError (const juce::String& id, juce::DynamicObject& errorObject)
        {
            auto* publicError = new juce::DynamicObject();
            publicError->setProperty ("code", errorObject.getProperty ("__error"));
            publicError->setProperty ("message", errorObject.getProperty ("message"));

            for (auto& property : errorObject.getProperties())
            {
                const auto name = property.name.toString();

                if (name != "__error" && name != "message")
                    publicError->setProperty (property.name, property.value);
            }

            return juce::JSON::toString (object ({ { "id", id },
                                                   { "ok", false },
                                                   { "error", juce::var (publicError) } }),
                                         true);
        }

        static juce::String getString (juce::DynamicObject& object, const juce::Identifier& name, const juce::String& fallback)
        {
            auto value = object.getProperty (name);
            return value.isVoid() ? fallback : value.toString();
        }

        static int getInt (juce::DynamicObject& object, const juce::Identifier& name, int fallback)
        {
            auto value = object.getProperty (name);
            return value.isVoid() ? fallback : (int) value;
        }

        static double getDouble (juce::DynamicObject& object, const juce::Identifier& name, double fallback)
        {
            auto value = object.getProperty (name);
            return value.isVoid() ? fallback : (double) value;
        }

        static bool parseFiniteDoubleString (juce::String text, double& result)
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

            if (!sawDigit)
                return false;

            result = text.getDoubleValue();
            return std::isfinite (result);
        }

        static bool getFiniteDouble (const juce::var& value, double& result)
        {
            if (value.isVoid() || value.isBool())
                return false;

            if (value.isString())
                return parseFiniteDoubleString (value.toString(), result);

            result = (double) value;
            return std::isfinite (result);
        }

        static bool getBool (juce::DynamicObject& object, const juce::Identifier& name, bool fallback)
        {
            auto value = object.getProperty (name);
            return value.isVoid() ? fallback : (bool) value;
        }

        static juce::String defaultSessionName()
        {
            return defaultAutomationSessionName();
        }

        static juce::File sessionsDirectory()
        {
            return NativeServices::sessionsDirectory();
        }

        static void restrictFilePermissions (const juce::File& file, int permissions)
        {
            NativeServices::restrictFilePermissions (file, permissions);
        }

        void writeAdvertisement()
        {
            auto sessionFileName = options.sessionName.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");

            if (sessionFileName.isEmpty())
                sessionFileName = "session";

            advertisementFile = sessionsDirectory().getChildFile (juce::String (currentProcessId()) + "-" + sessionFileName + ".json");

            auto* data = new juce::DynamicObject();
            data->setProperty ("pid", currentProcessId());
            data->setProperty ("session", options.sessionName);
            data->setProperty ("root", root != nullptr ? componentString (root.getComponent()) : juce::String());
            data->setProperty ("host", "127.0.0.1");
            data->setProperty ("port", boundPort);
            data->setProperty ("token", options.authToken);
            data->setProperty ("createdAt", juce::Time::getCurrentTime().toISO8601 (true));

            advertisementFile.replaceWithText (juce::JSON::toString (juce::var (data), true));
            restrictFilePermissions (advertisementFile, 0600);
        }

        static int currentProcessId()
        {
            return NativeServices::currentProcessId();
        }

        void removeAdvertisement()
        {
            if (advertisementFile.existsAsFile())
                advertisementFile.deleteFile();
        }
