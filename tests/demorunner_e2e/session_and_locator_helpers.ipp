        void clearEvidenceDirectory()
        {
            for (juce::RangedDirectoryIterator entry (evidenceDirectory, false, "*", juce::File::findFiles);
                 entry != juce::RangedDirectoryIterator();
                 ++entry)
            {
                entry->getFile().deleteFile();
            }
        }

        void cleanupSessionFiles()
        {
            auto sessionDir = sessionsDirectory();

            if (!sessionDir.isDirectory())
                return;

            for (juce::RangedDirectoryIterator entry (sessionDir, false, "*" + juce::String (sessionName) + "*.json", juce::File::findFiles);
                 entry != juce::RangedDirectoryIterator();
                 ++entry)
            {
                entry->getFile().deleteFile();
            }
        }

        void startDemoRunner()
        {
            require (demoRunner.start (makeArgs ({ demoRunnerPath.getFullPathName() })),
                     "Could not launch DemoRunner: " + demoRunnerPath.getFullPathName());
        }

        void stopDemoRunner()
        {
            if (demoRunner.isRunning())
                demoRunner.kill();
        }

        void waitForSession()
        {
            const auto deadline = juce::Time::currentTimeMillis() + 20000;

            while (juce::Time::currentTimeMillis() < deadline)
            {
                if (!demoRunner.isRunning())
                    fail ("DemoRunner exited before advertising automation");

                auto result = runProcess (makeCliCommand ({ "-s", sessionName, "capabilities" }), "jucewright capabilities", false, 3000);

                if (result.exitCode == 0)
                    return;

                juce::Thread::sleep (250);
            }

            fail ("Timed out waiting for DemoRunner automation session");
        }

        struct ProcessResult
        {
            int exitCode = -1;
            juce::String output;
        };

        juce::StringArray makeCliCommand (std::initializer_list<juce::String> args) const
        {
            auto command = makeArgs ({ cliPath.getFullPathName() });

            for (const auto& arg : args)
                command.add (arg);

            return command;
        }

        juce::String runCli (std::initializer_list<juce::String> args)
        {
            return runCli (makeArgs (args));
        }

        juce::String runCli (juce::StringArray args)
        {
            juce::StringArray command;
            command.add (cliPath.getFullPathName());
            command.addArray (args);

            auto result = runProcess (command, "jucewright " + args.joinIntoString (" "), true, 15000);
            return result.output.trim();
        }

        bool tryRunCli (std::initializer_list<juce::String> args, int timeoutMs = 3000)
        {
            auto argArray = makeArgs (args);
            juce::StringArray command;
            command.add (cliPath.getFullPathName());
            command.addArray (argArray);

            return runProcess (command, "jucewright " + argArray.joinIntoString (" "), false, timeoutMs).exitCode == 0;
        }

        ProcessResult runProcess (const juce::StringArray& command, const juce::String& label, bool expectSuccess, int timeoutMs)
        {
            juce::ChildProcess process;
            ProcessResult result;
            require (process.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr),
                     "Could not start " + command.joinIntoString (" "));

            const auto deadline = juce::Time::currentTimeMillis() + timeoutMs;
            char buffer[4096] {};

            while (process.isRunning())
            {
                if (auto bytesRead = process.readProcessOutput (buffer, (int) sizeof (buffer)); bytesRead > 0)
                    result.output << juce::String::fromUTF8 (buffer, bytesRead);

                if (juce::Time::currentTimeMillis() > deadline)
                {
                    process.kill();
                    fail ("Timed out running " + label + "\n" + result.output);
                }

                juce::Thread::sleep (10);
            }

            for (;;)
            {
                const auto bytesRead = process.readProcessOutput (buffer, (int) sizeof (buffer));

                if (bytesRead <= 0)
                    break;

                result.output << juce::String::fromUTF8 (buffer, bytesRead);
            }

            result.exitCode = static_cast<int> (process.getExitCode());

            if (expectSuccess)
                require (result.exitCode == 0, label + " failed with exit code " + juce::String (result.exitCode) + "\n" + result.output);

            return result;
        }

        juce::var readSnapshot (int depth = 4)
        {
            auto parsed = parseJsonOutput (runCli ({ "-s", sessionName, "snapshot", "--full", "--format", "json", "--depth", juce::String (depth) }), "snapshot");
            asObject (parsed, "snapshot");
            return parsed;
        }

        juce::var readWindows()
        {
            auto parsed = parseJsonOutput (runCli ({ "-s", sessionName, "windows" }), "windows");
            asObject (parsed, "windows");
            return parsed;
        }

        int windowCount()
        {
            auto windows = asObject (readWindows(), "windows").getProperty ("windows");
            return windows.isArray() ? windows.getArray()->size() : 0;
        }

        juce::Rectangle<int> rootWindowBounds()
        {
            auto windows = asObject (readWindows(), "windows").getProperty ("windows");
            require (windows.isArray(), "windows did not return an array");

            for (const auto& window : *windows.getArray())
            {
                auto* object = window.getDynamicObject();

                if (object != nullptr && object->getProperty ("id").toString() == "root")
                    return boundsOf (window);
            }

            fail ("Could not find root automation window");
            return {};
        }

        void resizeRootWindow (int width, int height)
        {
            runCli ({ "-s", sessionName, "resize-window", "--w", juce::String (width), "--h", juce::String (height) });
            runCli ({ "-s", sessionName, "wait", "--ms", "250" });
        }

        juce::String secondaryWindowIdContaining (const juce::String& titleText)
        {
            auto windows = asObject (readWindows(), "windows").getProperty ("windows");
            require (windows.isArray(), "windows did not return an array");

            for (const auto& window : *windows.getArray())
            {
                auto* object = window.getDynamicObject();

                if (object == nullptr)
                    continue;

                const auto id = object->getProperty ("id").toString();
                const auto title = object->getProperty ("title").toString();

                if (id != "root" && title.contains (titleText))
                    return id;
            }

            fail ("Could not find secondary window containing title: " + titleText);
            return {};
        }

        juce::var readLocator (std::initializer_list<juce::String> locatorArgs)
        {
            auto args = makeArgs ({ "-s", sessionName, "locator", "--format", "json" });
            args.addArray (makeArgs (locatorArgs));

            auto parsed = parseJsonOutput (runCli (args), "locator");
            asObject (parsed, "locator");
            return parsed;
        }

        juce::var firstLocatorMatch (std::initializer_list<juce::String> locatorArgs, const juce::String& label)
        {
            auto locator = readLocator (locatorArgs);
            auto matches = asObject (locator, label + " locator").getProperty ("matches");
            require (matches.isArray() && !matches.getArray()->isEmpty(), label + " locator returned no matches");
            return matches.getArray()->getReference (0);
        }

        int locatorCount (std::initializer_list<juce::String> locatorArgs, const juce::String& label)
        {
            return (int) asObject (readLocator (locatorArgs), label + " locator").getProperty ("count");
        }

        static juce::var findNode (const juce::var& node, const std::function<bool (juce::DynamicObject&)>& predicate)
        {
            auto* object = node.getDynamicObject();

            if (object == nullptr)
                return {};

            if (predicate (*object))
                return node;

            auto children = object->getProperty ("children");

            if (children.isArray())
                for (const auto& child : *children.getArray())
                    if (auto found = findNode (child, predicate); ! found.isVoid())
                        return found;

            return {};
        }

        static juce::var findSnapshotNode (const juce::var& snapshot,
                                           const juce::String& label,
                                           const std::function<bool (juce::DynamicObject&)>& predicate)
        {
            auto tree = asObject (snapshot, "snapshot").getProperty ("tree");
            auto found = findNode (tree, predicate);
            require (! found.isVoid(), "Could not find DemoRunner node: " + label);
            return found;
        }

        static juce::String nodeString (const juce::var& node, const juce::Identifier& property)
        {
            return asObject (node, "node").getProperty (property).toString();
        }

        static juce::String nodeRef (const juce::var& node)
        {
            return nodeString (node, "ref");
        }

        static juce::Rectangle<int> boundsOf (const juce::var& node)
        {
            auto bounds = asObject (node, "node").getProperty ("bounds");
            auto& boundsObject = asObject (bounds, "bounds");

            return { (int) boundsObject.getProperty ("x"),
                     (int) boundsObject.getProperty ("y"),
                     (int) boundsObject.getProperty ("w"),
                     (int) boundsObject.getProperty ("h") };
        }

        static int nodeInt (const juce::var& node, const juce::Identifier& property)
        {
            return (int) asObject (node, "node").getProperty (property);
        }

        static bool isVisible (juce::DynamicObject& node)
        {
            return (bool) node.getProperty ("visible");
        }

        static bool hasClass (juce::DynamicObject& node, const juce::String& className)
        {
            return node.getProperty ("class").toString().contains (className);
        }

        juce::var visibleNodeByClassAndName (const juce::var& snapshot, const juce::String& className, const juce::String& name)
        {
            return findSnapshotNode (snapshot, className + "=" + name, [&] (juce::DynamicObject& node) {
                return isVisible (node)
                       && hasClass (node, className)
                       && node.getProperty ("name").toString() == name;
            });
        }

        juce::var visibleNodeByClass (const juce::var& snapshot, const juce::String& className)
        {
            return findSnapshotNode (snapshot, className, [&] (juce::DynamicObject& node) {
                return isVisible (node) && hasClass (node, className);
            });
        }

        juce::var visibleNodeContainingText (const juce::var& snapshot, const juce::String& text)
        {
            return findSnapshotNode (snapshot, text, [&] (juce::DynamicObject& node) {
                const auto haystack = node.getProperty ("name").toString()
                                      + " " + node.getProperty ("title").toString()
                                      + " " + node.getProperty ("value").toString();

                return isVisible (node) && haystack.contains (text);
            });
        }

        juce::var topLevelTabs (const juce::var& snapshot)
        {
            return findSnapshotNode (snapshot, "DemoContentComponent", [] (juce::DynamicObject& node) {
                return isVisible (node) && hasClass (node, "DemoContentComponent");
            });
        }

        void selectTopLevelTab (const juce::String& tabName)
        {
            auto tabs = topLevelTabs (readSnapshot());
            runCli ({ "-s", sessionName, "select-tab", nodeRef (tabs), "--name", tabName });
            juce::Thread::sleep (250);
        }

        void openDemosPanel()
        {
            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Browse Demos", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-locator", "--role", "list", "--visible", "--timeout-ms", "3000" });
            juce::Thread::sleep (250);
        }

        void clickVisibleListItem (const juce::String& name)
        {
            scrollListItemIntoView (name);
            runCli ({ "-s", sessionName, "wait-for-locator", "--role", "listItem", "--name", name, "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "click", "--role", "listItem", "--name", name, "--exact", "--force", "--timeout-ms", "3000" });
            juce::Thread::sleep (500);
        }

        void scrollListItemIntoView (const juce::String& name)
        {
            tryRunCli ({ "-s", sessionName, "select-option", "--class", "juce::ListBox", "--exact", "--text", name, "--timeout-ms", "1000" }, 3000);

            for (int listIndex = 0; listIndex < 4; ++listIndex)
            {
                tryRunCli ({ "-s", sessionName, "select-option", "--class", "juce::ListBox", "--exact", "--nth", juce::String (listIndex), "--text", name, "--timeout-ms", "1000" }, 3000);

                if (tryRunCli ({ "-s", sessionName, "wait-for-locator", "--role", "listItem", "--name", name, "--exact", "--timeout-ms", "300" }, 1000))
                    return;
            }

            if (tryRunCli ({ "-s", sessionName, "wait-for-locator", "--role", "listItem", "--name", name, "--exact", "--timeout-ms", "300" }, 1000))
                return;

            for (int attempt = 0; attempt < 80; ++attempt)
            {
                if (tryRunCli ({ "-s", sessionName, "wait-for-locator", "--role", "listItem", "--name", name, "--exact", "--timeout-ms", "300" }, 1000))
                    return;

                runCli ({ "-s", sessionName, "wheel", "231", "300", "--dy", attempt < 40 ? "-12" : "12" });
                runCli ({ "-s", sessionName, "wait", "--ms", "100" });
            }

            require (false, "Could not scroll list item into view: " + name);
        }

        static juce::String categoryScreenshotName (const juce::String& category)
        {
            return category.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789") + "-category.png";
        }

        void openCategory (const juce::String& category)
        {
            openDemosPanel();

            if (!tryRunCli ({ "-s", sessionName, "wait-for-locator", "--role", "listItem", "--name", category, "--exact", "--timeout-ms", "500" }, 1500))
                tryRunCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Previous", "--exact", "--timeout-ms", "1000" }, 3000);

            runCli ({ "-s", sessionName, "wait-for-locator", "--role", "listItem", "--name", category, "--exact", "--timeout-ms", "3000" });
            clickVisibleListItem (category);

            if (!capturedCategories.contains (category))
            {
                captureScreenshot (categoryScreenshotName (category));
                capturedCategories.add (category);
            }
        }

        void selectDemoFromCategory (const juce::String& category, const juce::String& demoFile)
        {
            openCategory (category);
            clickVisibleListItem (demoFile);
            runCli ({ "-s", sessionName, "wait", "--ms", "750" });
        }

        void assertCodeTabLoads()
        {
            selectTopLevelTab ("Code");
            runCli ({ "-s", sessionName, "wait-for-locator", "--class", "CodeEditorComponent", "--timeout-ms", "3000" });
            selectTopLevelTab ("Demo");
        }

        void selectDemoLocalTab (const juce::String& tabName)
        {
            auto tabs = visibleNodeByClass (readSnapshot (8), "DemoTabbedComponent");
            runCli ({ "-s", sessionName, "select-tab", nodeRef (tabs), "--name", tabName });
            runCli ({ "-s", sessionName, "wait", "--ms", "250" });
        }

        void clickWindowPoint (juce::Rectangle<int> bounds, int relativeX, int relativeY, const juce::String& target = "root")
        {
            runCli ({ "-s", sessionName, "click-xy",
                      juce::String (bounds.getX() + relativeX),
                      juce::String (bounds.getY() + relativeY),
                      "--target", target });
        }

        void clickVisibleText (const juce::String& text)
        {
            auto node = visibleNodeContainingText (readSnapshot (10), text);
            auto bounds = boundsOf (node);
            clickWindowPoint (bounds, bounds.getWidth() / 2, bounds.getHeight() / 2);
            runCli ({ "-s", sessionName, "wait", "--ms", "250" });
        }

        struct DemoCase
        {
            enum class Exercise
            {
                codeEditor,
                componentGrid,
                componentTransforms,
                dialogs,
                grid,
                images,
                fonts,
                audioSettings,
                gain,
                valueTrees,
                xmlAndJson,
                openGLApp,
                openGL,
                widgets,
                menus,
                windows,
                mdi,
                properties,
                keyMappings,
                openGL2D
            };

            const char* category = nullptr;
            const char* file = nullptr;
            const char* screenshot = nullptr;
            Exercise exercise = Exercise::codeEditor;
        };
