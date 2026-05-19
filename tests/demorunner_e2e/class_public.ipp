    public:
        DemoRunnerE2E()
            : demoRunnerPath (juce::String (JUCEWRIGHT_DEMORUNNER_EXECUTABLE)),
              cliPath (juce::String (JUCEWRIGHT_EXECUTABLE))
        {
            const auto artifactDir = juce::SystemStats::getEnvironmentVariable ("JUCEWRIGHT_DEMORUNNER_ARTIFACT_DIR", {});
            evidenceDirectory = artifactDir.isNotEmpty()
                                    ? juce::File (artifactDir)
                                    : tempDirectory().getChildFile ("jucewright-demorunner-e2e");
        }

        ~DemoRunnerE2E()
        {
            stopDemoRunner();
        }

        void run()
        {
            require (demoRunnerPath.existsAsFile(), "Missing DemoRunner executable: " + demoRunnerPath.getFullPathName());
            require (cliPath.existsAsFile(), "Missing jucewright executable: " + cliPath.getFullPathName());
            require (evidenceDirectory.createDirectory(), "Could not create evidence directory: " + evidenceDirectory.getFullPathName());
            clearEvidenceDirectory();

            cleanupSessionFiles();
            startDemoRunner();
            waitForSession();
            runCli ({ "list" });
            runCli ({ "-s", sessionName, "trace-start", "--file", "demorunner-trace.json" });

            auto startupSnapshot = runCli ({ "-s", sessionName, "snapshot", "--format", "text", "--depth", "8" });
            require (startupSnapshot.contains ("JUCE Logo"), "startup snapshot did not include the DemoRunner home page");
            captureScreenshot ("startup.png");

            runMcpSmoke();

            openDemosPanel();
            clickVisibleListItem ("GUI");
            runCli ({ "-s", sessionName, "wait-for-locator", "--role", "listItem", "--name", "AccessibilityDemo.h", "--exact", "--timeout-ms", "3000" });
            captureScreenshot ("gui-category.png");

            auto compactGuiSnapshot = runCli ({ "-s", sessionName, "snapshot", "--json", "--depth", "8" });
            auto fullGuiSnapshot = runCli ({ "-s", sessionName, "snapshot", "--full", "--json", "--depth", "8" });
            require (compactGuiSnapshot.length() < fullGuiSnapshot.length(),
                     "DemoRunner interesting snapshot should be smaller than the full snapshot");
            require (compactGuiSnapshot.contains ("AccessibilityDemo.h"),
                     "DemoRunner interesting snapshot should retain actionable list items");

            auto guiCount = parseJsonOutput (runCli ({ "-s", sessionName, "count", "--role", "listItem", "--name", "AccessibilityDemo.h", "--exact" }), "DemoRunner count");
            require ((int) asObject (guiCount, "DemoRunner count").getProperty ("count") == 1,
                     "DemoRunner count did not find AccessibilityDemo.h");

            auto guiDescribe = parseJsonOutput (runCli ({ "-s", sessionName, "describe", "--role", "listItem", "--name", "AccessibilityDemo.h", "--exact" }), "DemoRunner describe");
            require (asObject (guiDescribe, "DemoRunner describe").getProperty ("text").toString().contains ("click"),
                     "DemoRunner describe did not expose list item action hints");

            clickVisibleListItem ("AccessibilityDemo.h");
            runCli ({ "-s", sessionName, "wait-for-text", "Accessibility Demo", "--timeout-ms", "3000" });
            captureScreenshot ("accessibility-demo.png");
            exerciseAccessibilityDemo();

            selectTopLevelTab ("Code");
            runCli ({ "-s", sessionName, "wait-for-locator", "--class", "CodeEditorComponent", "--timeout-ms", "3000" });
            captureScreenshot ("accessibility-code.png");

            selectTopLevelTab ("Demo");
            runCli ({ "-s", sessionName, "wait-for-text", "Accessibility Demo", "--timeout-ms", "3000" });

            openDemosPanel();
            clickVisibleListItem ("FlexBoxDemo.h");
            runCli ({ "-s", sessionName, "wait-for-text", "flex-grow", "--timeout-ms", "3000" });
            captureScreenshot ("flexbox-demo.png");
            exerciseFlexBoxDemo();

            exerciseAdditionalDemoCoverage();

            selectTopLevelTab ("Settings");
            runCli ({ "-s", sessionName, "wait-for-text", "LookAndFeel:", "--timeout-ms", "3000" });
            captureScreenshot ("settings.png");
            exerciseSettings();

            openDemosPanel();
            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Home", "--exact" });
            runCli ({ "-s", sessionName, "wait-for-text", "JUCE Logo", "--timeout-ms", "3000" });
            captureScreenshot ("home.png");

            auto traceStop = parseJsonOutput (runCli ({ "-s", sessionName, "trace-stop" }), "trace-stop");
            auto& traceStopObject = asObject (traceStop, "trace-stop");
            require ((int) traceStopObject.getProperty ("events") >= 100, "DemoRunner trace did not record enough events");
            copyEvidenceFile (traceStopObject.getProperty ("trace").toString(), "demorunner-trace.json");
        }

    private:
        juce::File demoRunnerPath;
        juce::File cliPath;
        juce::File evidenceDirectory;
        juce::ChildProcess demoRunner;
        juce::StringArray capturedCategories;
