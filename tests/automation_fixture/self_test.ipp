    class AutomationFixtureSelfTest : public juce::Thread
    {
    public:
        explicit AutomationFixtureSelfTest (std::function<void (int)> onCompleteCallback)
            : juce::Thread ("Automation Fixture Self Test"),
              onComplete (std::move (onCompleteCallback)),
              cliPath (findJucewright())
        {
        }

        ~AutomationFixtureSelfTest() override
        {
            signalThreadShouldExit();
            stopThread (2000);
        }

        void run() override
        {
            try
            {
                runChecks();
                std::cout << "ok - CLI/MCP automation e2e passed\n";
                std::cout << "root screenshot: " << rootScreenshot.getFullPathName() << "\n";
                finish (0);
            }
            catch (const std::exception& e)
            {
                std::cerr << "automation_fixture self-test failed: " << e.what() << "\n";
                finish (1);
            }
        }

    private:
        std::function<void (int)> onComplete;
        juce::File cliPath;
        juce::File screenshotDirectory = juce::SystemStats::getEnvironmentVariable ("JUCEWRIGHT_SCREENSHOT_DIR", {}).isNotEmpty()
                                             ? juce::File (juce::SystemStats::getEnvironmentVariable ("JUCEWRIGHT_SCREENSHOT_DIR", {}))
                                             : tempDirectory();
        juce::File rootScreenshot;

        void finish (int returnCode)
        {
            auto callback = onComplete;
            juce::MessageManager::callAsync ([callback, returnCode] {
                if (callback)
                    callback (returnCode);
            });
        }

        void runChecks()
        {
            require (cliPath.existsAsFile(), "Missing jucewright CLI: " + cliPath.getFullPathName());
            runProfilePreparationCheck();
            waitForSession();

            auto listOutput = runCli ({ "list" });
            require (listOutput.contains (juce::String (sessionName) + " "), "CLI list did not show the automation_fixture process");
            auto listedPid = listOutput.fromFirstOccurrenceOf ("pid=", false, false)
                                       .upToFirstOccurrenceOf (" ", false, false)
                                       .trim();
            require (listedPid.isNotEmpty() && listedPid.containsOnly ("0123456789"), "CLI list did not expose a targetable pid\n" + listOutput);
            auto pidCapabilities = parseJsonOutput (runCli ({ "-s", listedPid, "capabilities" }), "pid capabilities");
            require (asObject (pidCapabilities, "pid capabilities").getProperty ("session").toString() == sessionName,
                     "CLI -s <pid> did not select the automation_fixture session");

            runMcpSmokeCheck();

            auto snapshot = readSnapshot();
            auto capabilities = parseJsonOutput (runCli ({ "-s", sessionName, "capabilities" }), "capabilities");
            auto& capabilitiesObject = asObject (capabilities, "capabilities");
            require ((int) capabilitiesObject.getProperty ("protocolVersion") == 1, "capabilities returned the wrong protocol version");

            auto securityValue = capabilitiesObject.getProperty ("security");
            auto& security = asObject (securityValue, "capabilities.security");
            require ((bool) security.getProperty ("allowInput"), "capabilities did not expose allowInput=true");
            require ((bool) security.getProperty ("allowMutation"), "capabilities did not expose allowMutation=true");
            require ((bool) security.getProperty ("allowFileWrite"), "capabilities did not expose allowFileWrite=true");
            require (security.getProperty ("artifactRoot").toString() == screenshotDirectory.getFullPathName(),
                     "capabilities returned the wrong artifact root");

            runCli ({ "-s", sessionName, "wait", "--ms", "5500" });

            auto windows = parseJsonOutput (runCli ({ "-s", sessionName, "windows" }), "windows");
            auto windowsArray = asObject (windows, "windows").getProperty ("windows");
            require (windowsArray.isArray() && windowsArray.getArray()->size() == 1, "windows should expose the owned root window");
            require (asObject (windowsArray.getArray()->getReference (0), "root window").getProperty ("id").toString() == "root",
                     "root window id should be root");
            const auto rootWindowBeforeResize = boundsOf (windowsArray.getArray()->getReference (0));
            runCli ({ "-s", sessionName, "resize-window", "--w", "700", "--h", "460" });
            windows = parseJsonOutput (runCli ({ "-s", sessionName, "windows" }), "windows after resize-window");
            windowsArray = asObject (windows, "windows after resize-window").getProperty ("windows");
            auto rootWindowAfterResize = boundsOf (windowsArray.getArray()->getReference (0));
            require (rootWindowAfterResize.getWidth() != rootWindowBeforeResize.getWidth()
                         || rootWindowAfterResize.getHeight() != rootWindowBeforeResize.getHeight(),
                     "resize-window did not change the root window bounds");
            runCli ({ "-s", sessionName, "resize-window", "--w", "640", "--h", "420" });

            auto traceFile = screenshotDirectory.getChildFile ("jucewright-trace.json");
            runCli ({ "-s", sessionName, "trace-start", "--file", traceFile.getFullPathName() });
            runCli ({ "-s", sessionName, "snapshot", "--format", "json", "--depth", "4" });
            auto tracedFailure = runCliExpectFailure ({ "-s", sessionName, "click", "--component-id", "trace.missing", "--timeout-ms", "50" });
            require (tracedFailure.contains ("Timed out") || tracedFailure.contains ("Locator did not match"),
                     "expected traced missing locator click to fail\n" + tracedFailure);
            auto traceStop = parseJsonOutput (runCli ({ "-s", sessionName, "trace-stop" }), "trace-stop");
            require ((int) asObject (traceStop, "trace-stop").getProperty ("events") >= 2, "trace-stop did not record events");
            auto traceJson = juce::JSON::parse (traceFile.loadFileAsString());
            auto traceEvents = asObject (traceJson, "trace file").getProperty ("events");
            require (traceEvents.isArray() && !traceEvents.getArray()->isEmpty(), "trace file did not contain events");
            bool foundFailedTraceEvent = false;
            bool foundStructuredTraceEvent = false;

            for (const auto& traceEvent : *traceEvents.getArray())
            {
                auto& eventObject = asObject (traceEvent, "trace event");
                auto params = eventObject.getProperty ("params");
                auto result = eventObject.getProperty ("result");
                auto* resultObject = result.getDynamicObject();

                if (resultObject != nullptr && resultObject->getProperty ("text").isString())
                    require (resultObject->getProperty ("text").toString().length() <= 515,
                             "trace result text summary should be capped");

                foundStructuredTraceEvent = foundStructuredTraceEvent
                                            || ((double) eventObject.getProperty ("elapsedMs") >= 0.0
                                                && params.isObject()
                                                && result.isObject());
                foundFailedTraceEvent = foundFailedTraceEvent
                                        || (!(bool) eventObject.getProperty ("ok")
                                            && eventObject.getProperty ("error").toString().isNotEmpty()
                                            && eventObject.getProperty ("message").toString().isNotEmpty());
            }

            require (foundStructuredTraceEvent, "trace file did not include structured params/result/timing fields");
            require (foundFailedTraceEvent, "trace file did not include the expected failed action event");

            auto interestingText = runCli ({ "-s", sessionName, "snapshot" });
            auto fullText = runCli ({ "-s", sessionName, "snapshot", "--full", "--depth", "12" });
            require (interestingText.contains ("actions="), "default interesting snapshot should include action hints\n" + interestingText);
            require (interestingText.contains ("fixture.tabs"), "default interesting snapshot should include named controls\n" + interestingText);
            require (interestingText.length() < fullText.length(), "interesting snapshot should be smaller than full snapshot");

            auto interestingSnapshot = parseJsonOutput (runCli ({ "-s", sessionName, "snapshot", "--json", "--depth", "12" }), "interesting snapshot");
            auto& interestingObject = asObject (interestingSnapshot, "interesting snapshot");
            require (interestingObject.getProperty ("mode").toString() == "interesting", "snapshot should default to interesting mode");
            require (!findByComponentName (interestingSnapshot, "controls.slider").isVoid(),
                     "interesting snapshot should retain visible sliders");
            require (findByComponentName (interestingSnapshot, "editor.text").isVoid(),
                     "interesting snapshot should omit hidden tab contents by default");

            auto snapshotAgain = readSnapshot();
            auto& snapshotObject = asObject (snapshot, "snapshot");
            auto& snapshotAgainObject = asObject (snapshotAgain, "snapshotAgain");
            auto initialStateHash = snapshotObject.getProperty ("stateHash").toString();
            require (initialStateHash.isNotEmpty(), "snapshot is missing stateHash");
            require (initialStateHash == snapshotAgainObject.getProperty ("stateHash").toString(),
                     "repeated snapshots without UI changes should keep the same stateHash");
            require ((int) snapshotObject.getProperty ("generation") != (int) snapshotAgainObject.getProperty ("generation"),
                     "repeated snapshots should still advance the ref generation");
            snapshot = snapshotAgain;

            require (!findByComponentName (snapshot, "fixture.tabs").isVoid(), "snapshot is missing top-level tabs");
            require (!findByComponentName (snapshot, "controls.slider").isVoid(), "snapshot is missing the controls slider");

            auto& topTabs = asObject (findByComponentName (snapshot, "fixture.tabs"), "fixture.tabs");
            auto topTabNames = topTabs.getProperty ("tabNames");
            require (topTabNames.isArray() && topTabNames.getArray()->size() == 3, "snapshot did not expose fixture tab names");
            require (topTabs.getProperty ("currentTab").toString() == "Controls", "snapshot did not expose current fixture tab");

            auto& sliderMetadata = asObject (findByComponentName (snapshot, "controls.slider"), "controls.slider");
            require ((double) sliderMetadata.getProperty ("minimum") == 0.0, "slider metadata did not expose minimum");
            require ((double) sliderMetadata.getProperty ("maximum") == 100.0, "slider metadata did not expose maximum");
            require ((double) sliderMetadata.getProperty ("interval") == 1.0, "slider metadata did not expose interval");

            auto& comboMetadata = asObject (findByComponentName (snapshot, "controls.combo"), "controls.combo");
            auto comboOptions = comboMetadata.getProperty ("options");
            require (comboOptions.isArray() && comboOptions.getArray()->size() == 3, "combo metadata did not expose all options");
            require (comboMetadata.getProperty ("selectedId").toString() == "1", "combo metadata did not expose selected id");
            require (comboMetadata.getProperty ("selectedText").toString() == "Alpha", "combo metadata did not expose selected text");

            auto& listMetadata = asObject (findByComponentName (snapshot, "controls.optionList"), "controls.optionList");
            auto listOptions = listMetadata.getProperty ("options");
            require ((int) listMetadata.getProperty ("rowCount") == 3, "ListBox metadata did not expose row count");
            require (listOptions.isArray() && listOptions.getArray()->size() == 3, "ListBox metadata did not expose row options");

            auto& editorMetadata = asObject (findByComponentName (snapshot, "editor.text"), "editor.text");
            require ((bool) editorMetadata.getProperty ("editable"), "TextEditor metadata did not expose editable=true");
            require (! (bool) editorMetadata.getProperty ("readOnly"), "TextEditor metadata did not expose readOnly=false");

            auto buttonLocator = readLocator ({ "--role", "button", "--name", "Go Editor" });
            require ((int) asObject (buttonLocator, "button locator").getProperty ("count") == 1,
                     "role/name locator did not find Go Editor");

            auto nearbyActionLocator = readLocator ({ "--name", "Nearby Action", "--exact" });
            require ((int) asObject (nearbyActionLocator, "nearby action locator").getProperty ("count") == 1,
                     "nearby button text should not label adjacent unrelated controls");

            auto compositeToggleLocator = readLocator ({ "--role", "toggleButton", "--name", "Composite MIDI", "--exact" });
            require ((int) asObject (compositeToggleLocator, "composite toggle locator").getProperty ("count") == 1,
                     "generic inner toggle should inherit the composite parent name");

            auto sliderLocator = readLocator ({ "--test-id", "controls.slider" });
            require ((int) asObject (sliderLocator, "slider locator").getProperty ("count") == 1,
                     "test id locator did not find controls.slider");

            auto valueLocator = readLocator ({ "--value", "25" });
            require ((int) asObject (valueLocator, "value locator").getProperty ("count") >= 1,
                     "value locator did not find slider value 25");

            auto countResult = parseJsonOutput (runCli ({ "-s", sessionName, "count", "--role", "button", "--name", "Duplicate" }), "count");
            require ((int) asObject (countResult, "count result").getProperty ("count") == 2,
                     "count did not expose the duplicate button count");

            auto refSnapshot = readSnapshot();
            auto refBeforeCount = refByComponentName (refSnapshot, "nav.editor");
            runCli ({ "-s", sessionName, "count", "--role", "button" });
            runCli ({ "-s", sessionName, "click", refBeforeCount });
            snapshot = readSnapshot();
            require (!findByComponentName (snapshot, "editor.text").isVoid(),
                     "count should not invalidate refs from the previous snapshot");
            runCli ({ "-s", sessionName, "select-tab", "--component-name", "fixture.tabs", "--name", "Controls" });
            snapshot = readSnapshot();

            auto scopedByLocator = parseJsonOutput (runCli ({ "-s", sessionName, "snapshot", "--json", "--component-id", "controls.slider" }), "scoped locator snapshot");
            require (asObject (asObject (scopedByLocator, "scoped snapshot").getProperty ("tree"), "scoped tree").getProperty ("componentName").toString() == "controls.slider",
                     "locator-scoped snapshot did not return the slider subtree");

            auto describedSlider = parseJsonOutput (runCli ({ "-s", sessionName, "describe", "--component-id", "controls.slider" }), "describe slider");
            auto& describedSliderObject = asObject (describedSlider, "describe slider");
            require (describedSliderObject.getProperty ("text").toString().contains ("set_value"),
                     "describe did not expose slider action hints\n" + juce::JSON::toString (describedSlider, true));

            auto hiddenLocator = readLocator ({ "--component-name", "editor.text", "--hidden" });
            require ((int) asObject (hiddenLocator, "hidden locator").getProperty ("count") == 1,
                     "hidden locator did not find editor.text before its tab was selected");

            auto hiddenWaitFailure = runCliExpectFailure ({ "-s", sessionName, "wait-for-locator", "--component-name", "editor.text", "--timeout-ms", "100" });
            require (hiddenWaitFailure.contains ("Timed out") || hiddenWaitFailure.contains ("Locator did not match"),
                     "wait-for-locator should default to visible components\n" + hiddenWaitFailure);
            runCli ({ "-s", sessionName, "wait-for-locator", "--component-name", "editor.text", "--hidden", "--timeout-ms", "500" });

            auto hiddenTextFailure = runCliExpectFailure ({ "-s", sessionName, "wait-for-text", "--exact", "Editor Page", "--timeout-ms", "100" });
            require (hiddenTextFailure.contains ("Timed out") || hiddenTextFailure.contains ("Text was not found"),
                     "wait-for-text should default to visible text\n" + hiddenTextFailure);
            runCli ({ "-s", sessionName, "wait-for-text", "--hidden", "--exact", "Editor Page", "--timeout-ms", "500" });

            auto disabledLocator = readLocator ({ "--component-name", "controls.disabled", "--disabled" });
            require ((int) asObject (disabledLocator, "disabled locator").getProperty ("count") == 1,
                     "disabled locator did not find controls.disabled");

            auto disabledClick = runCliExpectFailure ({ "-s", sessionName, "click", "--component-name", "controls.disabled", "--timeout-ms", "100" });
            require (disabledClick.contains ("target_disabled") || disabledClick.contains ("disabled"),
                     "disabled target should fail actionability\n" + disabledClick);

            auto trialClick = runCli ({ "-s", sessionName, "click", "--component-name", "nav.editor", "--trial" });
            require (trialClick.contains ("actionability"), "trial click should report actionability without executing\n" + trialClick);

            auto stillHiddenLocator = readLocator ({ "--component-name", "editor.text", "--hidden" });
            require ((int) asObject (stillHiddenLocator, "still hidden locator").getProperty ("count") == 1,
                     "trial click should not navigate to the editor page");

            auto strictFailure = runCliExpectFailure ({ "-s", sessionName, "click", "--role", "button", "--name", "Duplicate" });
            require (strictFailure.contains ("strict") || strictFailure.contains ("matched 2"),
                     "duplicate locator should fail strict mode\n" + strictFailure);

            auto deniedScreenshot = screenshotDirectory.getSiblingFile ("jucewright-denied.png");
            auto deniedOutput = runCliExpectFailure ({ "-s", sessionName, "screenshot", "--target", "root", "--file", deniedScreenshot.getFullPathName() });
            require (deniedOutput.contains ("artifact root") || deniedOutput.contains ("artifact_path_denied"),
                     "screenshot outside artifact root should fail with artifact_path_denied\n" + deniedOutput);

            snapshot = readSnapshot();

            rootScreenshot = screenshotDirectory.getChildFile ("jucewright-e2e-root.png");
            runCli ({ "-s", sessionName, "screenshot", "--target", "root", "--file", rootScreenshot.getFullPathName() });
            assertPng (rootScreenshot, "root screenshot");

            auto screenshotMetadata = parseJsonOutput (runCli ({ "-s", sessionName, "screenshot", "--target", "root" }), "screenshot metadata");
            require (asObject (screenshotMetadata, "screenshot metadata").getProperty ("base64").isVoid(),
                     "CLI screenshot should omit base64 unless --base64 is passed");
            auto screenshotWithBase64 = parseJsonOutput (runCli ({ "-s", sessionName, "screenshot", "--target", "root", "--base64" }), "screenshot base64");
            require (asObject (screenshotWithBase64, "screenshot base64").getProperty ("base64").toString().isNotEmpty(),
                     "CLI screenshot --base64 should include encoded PNG bytes");
            auto invalidScaleOutput = runCliExpectFailure ({ "-s", sessionName, "screenshot", "--target", "root", "--scale", "0" });
            require (invalidScaleOutput.contains ("invalid_screenshot_scale"),
                     "invalid screenshot scale should fail clearly\n" + invalidScaleOutput);

            auto nativeRootScreenshot = screenshotDirectory.getChildFile ("jucewright-e2e-root-native.png");
            runCli ({ "-s", sessionName, "screenshot", "--target", "root", "--source", "native", "--file", nativeRootScreenshot.getFullPathName(), "--no-base64" });
            assertPng (nativeRootScreenshot, "native root screenshot");

            auto invalidSourceOutput = runCliExpectFailure ({ "-s", sessionName, "screenshot", "--target", "root", "--source", "bogus", "--file", rootScreenshot.getFullPathName() });
            require (invalidSourceOutput.contains ("invalid_screenshot_source") || invalidSourceOutput.contains ("source must be"),
                     "invalid screenshot source should fail clearly\n" + invalidSourceOutput);

            auto buttonScreenshot = screenshotDirectory.getChildFile ("jucewright-e2e-button.png");
            auto editorButton = findByComponentName (snapshot, "nav.editor");
            auto editorButtonBounds = boundsOf (editorButton);
            auto editorButtonRef = asObject (editorButton, "nav.editor").getProperty ("ref").toString();

            auto describedByRef = parseJsonOutput (runCli ({ "-s",
                                                             sessionName,
                                                             "describe",
                                                             editorButtonRef }),
                                                   "describe ref");
            require (asObject (describedByRef, "describe ref").getProperty ("text").toString().contains ("click"),
                     "describe <ref> did not expose button action hints");
            editorButtonRef = asObject (asObject (describedByRef, "describe ref").getProperty ("detail"), "describe detail").getProperty ("ref").toString();

            auto scopedByRef = parseJsonOutput (runCli ({ "-s",
                                                          sessionName,
                                                          "snapshot",
                                                          "--json",
                                                          "--ref",
                                                          editorButtonRef }),
                                                "scoped ref snapshot");
            auto& scopedRefTree = asObject (asObject (scopedByRef, "ref scoped snapshot").getProperty ("tree"), "ref scoped tree");
            require (scopedRefTree.getProperty ("componentName").toString() == "nav.editor",
                     "ref-scoped snapshot did not return the requested component");
            editorButtonRef = scopedRefTree.getProperty ("ref").toString();

            runCli ({ "-s", sessionName, "screenshot", "--ref", editorButtonRef, "--file", buttonScreenshot.getFullPathName() });
            assertPng (buttonScreenshot, "button screenshot");
            assertPngSize (buttonScreenshot, editorButtonBounds.getWidth(), editorButtonBounds.getHeight(), "button screenshot");

            auto locatorButtonScreenshot = screenshotDirectory.getChildFile ("jucewright-e2e-button-locator.png");
            runCli ({ "-s", sessionName, "screenshot", "--component-id", "nav.editor", "--file", locatorButtonScreenshot.getFullPathName(), "--no-base64" });
            assertPngSize (locatorButtonScreenshot, editorButtonBounds.getWidth(), editorButtonBounds.getHeight(), "locator button screenshot");

            auto clippedScreenshot = screenshotDirectory.getChildFile ("jucewright-e2e-clip.png");
            runCli ({ "-s",
                      sessionName,
                      "screenshot",
                      "--target",
                      "root",
                      "--clip-x",
                      "0",
                      "--clip-y",
                      "0",
                      "--clip-w",
                      "50",
                      "--clip-h",
                      "40",
                      "--file",
                      clippedScreenshot.getFullPathName(),
                      "--no-base64" });
            assertPngSize (clippedScreenshot, 50, 40, "clipped screenshot");

            clickXYAtNode (findByComponentName (snapshot, "controls.power"));
            snapshot = readSnapshot();
            require (asObject (snapshot, "snapshot after click").getProperty ("stateHash").toString() != initialStateHash,
                     "clicking the power button should change the semantic stateHash");
            require ((bool) asObject (findByComponentName (snapshot, "controls.power"), "controls.power").getProperty ("toggleState"),
                     "click-xy did not toggle the power button");
            assertStatus (snapshot, "Status: Power On");
            runCli ({ "-s", sessionName, "wait-for-ref", refByComponentName (snapshot, "controls.slider"), "--timeout-ms", "500" });
            runCli ({ "-s", sessionName, "wait-for-text", "Status: Power On", "--timeout-ms", "500" });
            runCli ({ "-s", sessionName, "wait-for-snapshot-change", "--state-hash", initialStateHash, "--timeout-ms", "500" });
            readLocator ({ "--component-id", "controls.slider" });
            runCli ({ "-s", sessionName, "wait-for-locator", "--component-id", "controls.slider", "--timeout-ms", "500" });

            runCli ({ "-s", sessionName, "uncheck", "--component-id", "controls.power" });
            snapshot = readSnapshot();
            require (! (bool) asObject (findByComponentName (snapshot, "controls.power"), "controls.power").getProperty ("toggleState"),
                     "semantic uncheck did not clear the power button");

            runCli ({ "-s", sessionName, "check", "--component-id", "controls.power" });
            snapshot = readSnapshot();
            require ((bool) asObject (findByComponentName (snapshot, "controls.power"), "controls.power").getProperty ("toggleState"),
                     "semantic check did not set the power button");
            assertStatus (snapshot, "Status: Power On");

            runCli ({ "-s", sessionName, "set-checked", "--component-id", "controls.power", "false" });
            snapshot = readSnapshot();
            require (! (bool) asObject (findByComponentName (snapshot, "controls.power"), "controls.power").getProperty ("checked"),
                     "semantic set-checked false did not clear the power button");

            runCli ({ "-s", sessionName, "set-checked", "--component-id", "controls.power", "true" });
            snapshot = readSnapshot();
            require ((bool) asObject (findByComponentName (snapshot, "controls.power"), "controls.power").getProperty ("checked"),
                     "semantic set-checked true did not set the power button");
            assertStatus (snapshot, "Status: Power On");
            auto invalidChecked = runCliExpectFailure ({ "-s", sessionName, "set-checked", "--component-id", "controls.power", "maybe" });
            require (invalidChecked.contains ("set-checked requires true or false"),
                     "invalid set-checked value should fail clearly\n" + invalidChecked);

            auto retainedSliderRef = refByComponentName (snapshot, "controls.slider");
            runCli ({ "-s", sessionName, "set-value", "--component-id", "controls.slider", "44" });
            snapshot = readSnapshot();
            require (juce::roundToInt (valueOf (findByComponentName (snapshot, "controls.slider"))) == 44,
                     "semantic set-value did not update the slider");
            assertStatus (snapshot, "Status: Slider 44");
            runCli ({ "-s", sessionName, "wait-for-value", "--component-id", "controls.slider", "--value", "44", "--timeout-ms", "500" });
            runCli ({ "-s", sessionName, "set-value", retainedSliderRef, "45" });
            snapshot = readSnapshot();
            require (juce::roundToInt (valueOf (findByComponentName (snapshot, "controls.slider"))) == 45,
                     "retained ref did not survive an intervening action snapshot");
            auto invalidSliderValue = runCliExpectFailure ({ "-s", sessionName, "set-value", "--component-id", "controls.slider", "not-a-number" });
            require (invalidSliderValue.contains ("invalid_value"),
                     "invalid slider set-value should fail clearly\n" + invalidSliderValue);

            runCli ({ "-s", sessionName, "select-option", "--component-id", "controls.combo", "--text", "Beta" });
            snapshot = readSnapshot();
            require (asObject (findByComponentName (snapshot, "controls.combo"), "controls.combo").getProperty ("value").toString() == "Beta",
                     "semantic select-option did not select Beta");
            auto invalidOptionIndex = runCliExpectFailure ({ "-s", sessionName, "select-option", "--component-id", "controls.combo", "--index", "99" });
            require (invalidOptionIndex.contains ("option_not_found") || invalidOptionIndex.contains ("out of range"),
                     "invalid combo option index should fail clearly\n" + invalidOptionIndex);

            runCli ({ "-s", sessionName, "select-option", "--component-id", "controls.optionList", "--text", "Green" });
            snapshot = readSnapshot();
            require ((int) asObject (findByComponentName (snapshot, "controls.optionList"), "controls.optionList").getProperty ("selectedRow") == 1,
                     "ListBox metadata did not expose selected Green row");
            require (asObject (findByComponentName (snapshot, "controls.optionList"), "controls.optionList").getProperty ("selectedText").toString() == "Green",
                     "ListBox metadata did not expose selected Green text");
            assertStatus (snapshot, "Status: List Green");

            runCli ({ "-s", sessionName, "select-option", "--component-id", "controls.optionList", "--index", "2" });
            snapshot = readSnapshot();
            require ((int) asObject (findByComponentName (snapshot, "controls.optionList"), "controls.optionList").getProperty ("selectedRow") == 2,
                     "ListBox metadata did not expose selected Blue row");
            assertStatus (snapshot, "Status: List Blue");

            auto sliderBefore = valueOf (findByComponentName (snapshot, "controls.slider"));
            dragRef (refByComponentName (snapshot, "controls.slider"), 90, 0, 6);
            snapshot = readSnapshot();
            auto sliderAfter = valueOf (findByComponentName (snapshot, "controls.slider"));
            require (sliderAfter > sliderBefore,
                     "slider drag did not increase value: " + juce::String (sliderBefore) + " -> " + juce::String (sliderAfter));
            assertStatus (snapshot, "Status: Slider");

            runCli ({ "-s", sessionName, "click", "--component-id", "nav.editor" });
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: Editor");
            require (!findByComponentName (snapshot, "editor.text").isVoid(), "editor page did not expose its text editor");

            runCli ({ "-s", sessionName, "fill", "--component-id", "editor.text", "semantic fill" });
            snapshot = readSnapshot();
            clickRef (refByComponentName (snapshot, "editor.apply"));
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: Applied semantic fill");

            auto editorRef = refByComponentName (snapshot, "editor.text");
            runCli ({ "-s", sessionName, "clear", editorRef });
            snapshot = readSnapshot();
            require (asObject (findByComponentName (snapshot, "editor.text"), "editor.text").getProperty ("value").toString().isEmpty(),
                     "semantic clear did not empty the editor");
            editorRef = refByComponentName (snapshot, "editor.text");
            typeRef (editorRef, "hello from automation");
            snapshot = readSnapshot();
            pressRef (refByComponentName (snapshot, "editor.text"), "!");
            snapshot = readSnapshot();
            clickRef (refByComponentName (snapshot, "editor.apply"));
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: Applied hello from automation!");

            pressRef (refByComponentName (snapshot, "editor.text"), "backspace");
            snapshot = readSnapshot();
            clickRef (refByComponentName (snapshot, "editor.apply"));
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: Applied hello from automation");

            clickRef (refByComponentName (snapshot, "nav.advanced"));
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: Advanced");
            require (!findByComponentName (snapshot, "advanced.tabs").isVoid(), "advanced page did not expose nested tabs");

            runCli ({ "-s", sessionName, "select-tab", "--component-id", "advanced.tabs", "--name", "Actions" });
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: Nested Actions");
            require (!findByComponentName (snapshot, "advanced.reset").isVoid(), "nested Actions tab did not expose Reset All");
            require (!findByComponentName (snapshot, "advanced.dragBox").isVoid(), "nested Actions tab did not expose Drag Box");

            auto dragBoxBefore = findByComponentName (snapshot, "advanced.dragBox");
            auto dragBoxBeforeBounds = boundsOf (dragBoxBefore);
            dragRef (asObject (dragBoxBefore, "advanced.dragBox").getProperty ("ref").toString(), 40, 15, 4);
            snapshot = readSnapshot();
            auto dragBoxAfterBounds = boundsOf (findByComponentName (snapshot, "advanced.dragBox"));
            require (dragBoxAfterBounds.getX() == dragBoxBeforeBounds.getX() + 40, "drag did not move Drag Box on the x axis");
            require (dragBoxAfterBounds.getY() == dragBoxBeforeBounds.getY() + 15, "drag did not move Drag Box on the y axis");
            assertStatus (snapshot, "steps=4");

            runCli ({ "-s", sessionName, "hover", juce::String (dragBoxAfterBounds.getCentreX()), juce::String (dragBoxAfterBounds.getCentreY()) });
            runCli ({ "-s", sessionName, "mouse-down", juce::String (dragBoxAfterBounds.getCentreX()), juce::String (dragBoxAfterBounds.getCentreY()) });
            runCli ({ "-s", sessionName, "mouse-up", juce::String (dragBoxAfterBounds.getCentreX()), juce::String (dragBoxAfterBounds.getCentreY()) });
            runCli ({ "-s", sessionName, "wheel", juce::String (dragBoxAfterBounds.getCentreX()), juce::String (dragBoxAfterBounds.getCentreY()), "--dy", "-1" });
            runCli ({ "-s",
                      sessionName,
                      "drag-xy",
                      juce::String (dragBoxAfterBounds.getCentreX()),
                      juce::String (dragBoxAfterBounds.getCentreY()),
                      juce::String (dragBoxAfterBounds.getCentreX() + 20),
                      juce::String (dragBoxAfterBounds.getCentreY() + 10),
                      "--steps",
                      "3" });
            snapshot = readSnapshot();
            auto dragBoxPointDragBounds = boundsOf (findByComponentName (snapshot, "advanced.dragBox"));
            require (dragBoxPointDragBounds.getX() == dragBoxAfterBounds.getX() + 20, "drag-xy did not move Drag Box on the x axis");
            require (dragBoxPointDragBounds.getY() == dragBoxAfterBounds.getY() + 10, "drag-xy did not move Drag Box on the y axis");
            assertStatus (snapshot, "steps=3");

            auto inputProbeBounds = boundsOf (findByComponentName (snapshot, "advanced.inputProbe"));
            auto expectedDragToBounds = dragBoxPointDragBounds.translated (inputProbeBounds.getCentreX() - dragBoxPointDragBounds.getCentreX(),
                                                                          inputProbeBounds.getCentreY() - dragBoxPointDragBounds.getCentreY());
            runCli ({ "-s",
                      sessionName,
                      "drag-to",
                      "--component-name",
                      "advanced.dragBox",
                      "--target-component-name",
                      "advanced.inputProbe",
                      "--steps",
                      "5" });
            snapshot = readSnapshot();
            auto dragBoxAfterDragToBounds = boundsOf (findByComponentName (snapshot, "advanced.dragBox"));
            require (dragBoxAfterDragToBounds.getX() == expectedDragToBounds.getX(), "drag-to did not move Drag Box to target x");
            require (dragBoxAfterDragToBounds.getY() == expectedDragToBounds.getY(), "drag-to did not move Drag Box to target y");
            assertStatus (snapshot, "steps=5");
            clickRef (refByComponentName (snapshot, "advanced.reset"));
            snapshot = readSnapshot();
            dragBoxAfterDragToBounds = boundsOf (findByComponentName (snapshot, "advanced.dragBox"));

            auto inputProbeBoundsBeforeMcpDrag = boundsOf (findByComponentName (snapshot, "advanced.inputProbe"));
            auto expectedMcpDragToBounds = dragBoxAfterDragToBounds.translated (inputProbeBoundsBeforeMcpDrag.getCentreX() - dragBoxAfterDragToBounds.getCentreX(),
                                                                               inputProbeBoundsBeforeMcpDrag.getCentreY() - dragBoxAfterDragToBounds.getCentreY());
            auto dragToOutput = runMcpBatch ({
                R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25"}})",
                R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"juce_drag_to","arguments":{"session":"automation_fixture","locator":{"componentName":"advanced.dragBox"},"targetLocator":{"componentName":"advanced.inputProbe"},"steps":2}}})"
            });
            auto dragToLines = juce::StringArray::fromLines (dragToOutput);
            require (dragToLines.size() >= 2, "MCP drag_to expected at least 2 response lines, got " + juce::String (dragToLines.size()) + "\n" + dragToOutput);
            assertMcpResult (parseMcpLine (dragToLines, 1), 2);
            snapshot = readSnapshot();
            auto dragBoxAfterMcpDragToBounds = boundsOf (findByComponentName (snapshot, "advanced.dragBox"));
            require (dragBoxAfterMcpDragToBounds.getX() == expectedMcpDragToBounds.getX(), "MCP drag_to did not move Drag Box to target x");
            require (dragBoxAfterMcpDragToBounds.getY() == expectedMcpDragToBounds.getY(), "MCP drag_to did not move Drag Box to target y");
            assertStatus (snapshot, "steps=2");
            clickRef (refByComponentName (snapshot, "advanced.reset"));
            snapshot = readSnapshot();

            runCli ({ "-s", sessionName, "dblclick", "--component-id", "advanced.inputProbe" });
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: DoubleClick");

            runCli ({ "-s", sessionName, "right-click", "--component-id", "advanced.inputProbe" });
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: RightClick");

            runCli ({ "-s", sessionName, "click", "--component-id", "advanced.inputProbe", "--click-count", "2" });
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: DoubleClick");

            runCli ({ "-s", sessionName, "click", "--component-id", "advanced.inputProbe", "--button", "right", "--position", "8,8" });
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: RightClick");

            auto invalidClickPosition = runCliExpectFailure ({ "-s", sessionName, "click", "--component-id", "advanced.inputProbe", "--position", "999,999" });
            require (invalidClickPosition.contains ("invalid_coordinate") || invalidClickPosition.contains ("outside target bounds"),
                     "click with an out-of-bounds target-local position should fail\n" + invalidClickPosition);

            runCli ({ "-s", sessionName, "press", "Control+K", "--component-id", "advanced.inputProbe" });
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: Key Ctrl+K");

            runCli ({ "-s", sessionName, "key-down", "Shift+X", "--component-id", "advanced.inputProbe" });
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: Key Shift+X");
            runCli ({ "-s", sessionName, "key-up", "Shift+X", "--component-id", "advanced.inputProbe" });
            snapshot = readSnapshot();

            clickRef (refByComponentName (snapshot, "advanced.reset"));
            snapshot = readSnapshot();
            assertStatus (snapshot, "Status: Reset");
        }

        juce::String runCli (std::initializer_list<juce::String> args)
        {
            return runCli (makeArgs (args));
        }

        void runProfilePreparationCheck()
        {
            auto home = tempDirectory().getNonexistentChildFile ("jucewright-profile", {});
            auto result = parseJsonOutput (runCli ({ "prepare-juce-profile",
                                                     "--home",
                                                     home.getFullPathName(),
                                                     "--app-name",
                                                     "AutomationFixture" }),
                                           "prepare-juce-profile");
            auto& resultObject = asObject (result, "prepare-juce-profile");
            auto settingsFile = juce::File (resultObject.getProperty ("settingsFile").toString());
            require (settingsFile.existsAsFile(), "prepare-juce-profile did not write a settings file");
            std::unique_ptr<juce::XmlElement> settings (juce::parseXML (settingsFile));
            require (settings != nullptr && settings->hasTagName ("PROPERTIES"), "prepared settings file is not JUCE properties XML");
            auto foundAudioSetup = false;

            for (auto* child : settings->getChildIterator())
                foundAudioSetup = foundAudioSetup || (child != nullptr && child->getStringAttribute ("name") == "audioSetup");

            require (foundAudioSetup, "prepare-juce-profile did not write an audioSetup reset");
            require ((bool) resultObject.getProperty ("clearedFilterState"), "prepare-juce-profile did not report clearedFilterState=true");
            home.deleteRecursively();
        }

        juce::String runCli (juce::StringArray args)
        {
            juce::StringArray command;
            command.add (cliPath.getFullPathName());
            command.addArray (args);

            return runProcess (command, "jucewright " + args.joinIntoString (" "), true);
        }

        juce::String runCliExpectFailure (std::initializer_list<juce::String> args)
        {
            juce::StringArray command;
            command.add (cliPath.getFullPathName());
            command.addArray (makeArgs (args));

            return runProcess (command, "jucewright " + makeArgs (args).joinIntoString (" "), false);
        }

        juce::String runProcess (const juce::StringArray& command, const juce::String& label, bool expectSuccess)
        {
            auto displayCommand = command.joinIntoString (" ");

            juce::ChildProcess process;
            require (process.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr),
                     "Could not start " + displayCommand);

            juce::String output;
            auto deadline = juce::Time::currentTimeMillis() + 10000;
            char buffer[4096] {};

            while (process.isRunning())
            {
                if (auto bytesRead = process.readProcessOutput (buffer, (int) sizeof (buffer)); bytesRead > 0)
                    output << juce::String::fromUTF8 (buffer, bytesRead);

                if (juce::Time::currentTimeMillis() > deadline || threadShouldExit())
                {
                    process.kill();
                    throw std::runtime_error (("Timed out running " + label + "\n" + output).toStdString());
                }

                juce::Thread::sleep (10);
            }

            for (;;)
            {
                auto bytesRead = process.readProcessOutput (buffer, (int) sizeof (buffer));

                if (bytesRead <= 0)
                    break;

                output << juce::String::fromUTF8 (buffer, bytesRead);
            }

            if (expectSuccess)
            {
                require (process.getExitCode() == 0,
                         label + " failed with exit code " + juce::String ((int) process.getExitCode()) + "\n" + output);
            }
            else
            {
                require (process.getExitCode() != 0,
                         label + " unexpectedly succeeded\n" + output);
            }

            return output;
        }

        juce::String runMcpBatch (std::initializer_list<juce::String> requests)
        {
            auto requestFile = tempDirectory().getNonexistentChildFile ("jucewright-mcp-requests", ".jsonl");
            juce::String requestText;

            for (const auto& request : requests)
                requestText << request << "\n";

            require (requestFile.replaceWithText (requestText), "Could not write MCP request file: " + requestFile.getFullPathName());

            juce::StringArray command;

        #if JUCE_WINDOWS
            command.add ("cmd");
            command.add ("/C");
            command.add ("type " + shellQuote (requestFile.getFullPathName()) + " | " + shellQuote (cliPath.getFullPathName()) + " mcp");
        #else
            command.add ("/bin/sh");
            command.add ("-c");
            command.add ("cat " + shellQuote (requestFile.getFullPathName()) + " | " + shellQuote (cliPath.getFullPathName()) + " mcp");
        #endif

            auto output = runProcess (command, "jucewright mcp", true);
            requestFile.deleteFile();
            return output;
        }

        juce::var parseMcpLine (const juce::StringArray& lines, int index)
        {
            require (juce::isPositiveAndBelow (index, lines.size()), "MCP response " + juce::String (index) + " was not written");

            auto parsed = juce::JSON::parse (lines[index]);
            asObject (parsed, "MCP response " + juce::String (index));
            return parsed;
        }

        juce::var assertMcpResult (const juce::var& response, int expectedId)
        {
            auto& responseObject = asObject (response, "MCP response");
            require ((int) responseObject.getProperty ("id") == expectedId,
                     "MCP response id mismatch: expected " + juce::String (expectedId)
                         + ", got " + responseObject.getProperty ("id").toString());
            require (responseObject.getProperty ("error").isVoid(), "MCP response returned an error: " + juce::JSON::toString (response, true));

            auto result = responseObject.getProperty ("result");
            asObject (result, "MCP result");
            return result;
        }

        void runMcpSmokeCheck()
        {
            auto output = runMcpBatch ({
                R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25"}})",
                R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})",
                R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"juce_capabilities","arguments":{"session":"automation_fixture"}}})",
                R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"juce_locator","arguments":{"session":"automation_fixture","locator":{"componentName":"controls.slider"}}}})",
                R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"juce_snapshot","arguments":{"session":"automation_fixture","depth":12}}})",
                R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"juce_count","arguments":{"session":"automation_fixture","locator":{"role":"button","name":"Duplicate"}}}})",
                R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"juce_describe","arguments":{"session":"automation_fixture","locator":{"componentName":"controls.slider"}}}})",
                R"({"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"juce_click","arguments":{"session":"automation_fixture","locator":{"componentName":"missing.button"},"timeoutMs":50}}})"
            });

            auto rawLines = juce::StringArray::fromLines (output);
            juce::StringArray lines;

            for (auto line : rawLines)
                if (line.trim().startsWithChar ('{'))
                    lines.add (line);

            require (lines.size() >= 8, "MCP smoke expected at least 8 response lines, got " + juce::String (lines.size()) + "\n" + output);

            auto initializeResult = assertMcpResult (parseMcpLine (lines, 0), 1);
            auto& initialize = asObject (initializeResult, "MCP initialize result");
            auto serverInfoValue = initialize.getProperty ("serverInfo");
            auto& serverInfo = asObject (serverInfoValue, "MCP serverInfo");
            require (serverInfo.getProperty ("name").toString() == "jucewright-mcp", "MCP initialize returned the wrong server name");

            auto toolsListResult = assertMcpResult (parseMcpLine (lines, 1), 2);
            auto& toolsList = asObject (toolsListResult, "MCP tools/list result");
            auto tools = toolsList.getProperty ("tools");
            require (tools.isArray(), "MCP tools/list did not return a tools array");

            auto findTool = [&tools] (const juce::String& toolName) -> juce::DynamicObject* {
                for (const auto& toolInfo : *tools.getArray())
                    if (auto* toolObject = toolInfo.getDynamicObject())
                        if (toolObject->getProperty ("name").toString() == toolName)
                            return toolObject;

                return nullptr;
            };

            auto requireTool = [&findTool] (const juce::String& toolName) -> juce::DynamicObject& {
                auto* toolObject = findTool (toolName);
                require (toolObject != nullptr, "MCP tools/list did not expose " + toolName);
                return *toolObject;
            };

            auto requireSchemaProperty = [&requireTool] (const juce::String& toolName, const juce::String& propertyName) {
                auto& toolObject = requireTool (toolName);
                auto& inputSchema = asObject (toolObject.getProperty ("inputSchema"), toolName + " inputSchema");
                auto& properties = asObject (inputSchema.getProperty ("properties"), toolName + " schema properties");
                auto property = properties.getProperty (propertyName);

                require (!property.isVoid(), toolName + " schema did not expose " + propertyName);

                if (propertyName == "timeoutMs")
                    require ((int) asObject (property, toolName + " timeoutMs schema").getProperty ("default") == 5000,
                             toolName + " timeoutMs schema should advertise the 5000ms default");
            };

            auto requireSchemaProperties = [&requireSchemaProperty] (const juce::String& toolName, std::initializer_list<const char*> propertyNames) {
                for (auto* propertyName : propertyNames)
                    requireSchemaProperty (toolName, propertyName);
            };

            for (auto* toolName : { "juce_snapshot",
                                    "juce_locator",
                                    "juce_count",
                                    "juce_describe",
                                    "juce_wait_for_text",
                                    "juce_dblclick",
                                    "juce_right_click",
                                    "juce_key_down",
                                    "juce_key_up",
                                    "juce_clear",
                                    "juce_set_checked",
                                    "juce_resize_window",
                                    "juce_drag_to" })
                requireTool (toolName);

            for (auto* toolName : { "juce_click",
                                    "juce_dblclick",
                                    "juce_right_click",
                                    "juce_type",
                                    "juce_fill",
                                    "juce_clear",
                                    "juce_press",
                                    "juce_key_down",
                                    "juce_key_up",
                                    "juce_check",
                                    "juce_uncheck",
                                    "juce_set_checked",
                                    "juce_set_value",
                                    "juce_select_option",
                                    "juce_select_tab",
                                    "juce_drag",
                                    "juce_drag_to" })
                requireSchemaProperties (toolName, { "timeoutMs", "force", "trial" });

            for (auto* toolName : { "juce_screenshot",
                                    "juce_click_xy",
                                    "juce_hover",
                                    "juce_mouse_move",
                                    "juce_mouse_down",
                                    "juce_mouse_up",
                                    "juce_wheel",
                                    "juce_drag_xy",
                                    "juce_wait_for_ref",
                                    "juce_wait_for_locator",
                                    "juce_wait_for_text",
                                    "juce_wait_for_value",
                                    "juce_wait_for_snapshot_change" })
                requireSchemaProperty (toolName, "timeoutMs");

            requireSchemaProperties ("juce_resize_window", { "w", "h" });

            auto capabilitiesCallResult = assertMcpResult (parseMcpLine (lines, 2), 3);
            auto& capabilitiesCall = asObject (capabilitiesCallResult, "MCP capabilities result");
            auto capabilitiesContent = capabilitiesCall.getProperty ("content");
            require (capabilitiesContent.isArray() && !capabilitiesContent.getArray()->isEmpty(), "MCP capabilities did not return content");
            require (asObject (capabilitiesContent.getArray()->getReference (0), "MCP capabilities content").getProperty ("text").toString().contains ("protocolVersion"),
                     "MCP capabilities content did not include protocolVersion");

            auto locatorCallResult = assertMcpResult (parseMcpLine (lines, 3), 4);
            auto& locatorCall = asObject (locatorCallResult, "MCP locator result");
            auto locatorContent = locatorCall.getProperty ("content");
            require (locatorContent.isArray() && !locatorContent.getArray()->isEmpty(), "MCP locator did not return content");
            require (asObject (locatorContent.getArray()->getReference (0), "MCP locator content").getProperty ("text").toString().contains ("controls.slider"),
                     "MCP locator content did not include controls.slider");

            auto snapshotCallResult = assertMcpResult (parseMcpLine (lines, 4), 5);
            auto& snapshotCall = asObject (snapshotCallResult, "MCP snapshot result");
            auto content = snapshotCall.getProperty ("content");
            require (content.isArray() && !content.getArray()->isEmpty(), "MCP snapshot did not return content");

            auto text = asObject (content.getArray()->getReference (0), "MCP snapshot content").getProperty ("text").toString();
            require (text.contains ("fixture.tabs"), "MCP snapshot content did not include the fixture tree");

            auto parsedMcpSnapshot = juce::JSON::parse (text);
            require (asObject (parsedMcpSnapshot, "MCP parsed snapshot").getProperty ("mode").toString() == "interesting",
                     "MCP snapshot should default to interesting JSON");

            auto countCallResult = assertMcpResult (parseMcpLine (lines, 5), 6);
            auto countText = asObject (asObject (countCallResult, "MCP count result").getProperty ("content").getArray()->getReference (0),
                                       "MCP count content")
                                 .getProperty ("text")
                                 .toString();
            auto parsedCount = juce::JSON::parse (countText);
            require ((int) asObject (parsedCount, "MCP parsed count").getProperty ("count") == 2,
                     "MCP count did not return the expected duplicate count");

            auto describeCallResult = assertMcpResult (parseMcpLine (lines, 6), 7);
            auto describeText = asObject (asObject (describeCallResult, "MCP describe result").getProperty ("content").getArray()->getReference (0),
                                          "MCP describe content")
                                    .getProperty ("text")
                                    .toString();
            require (describeText.contains ("set_value"), "MCP describe did not expose slider action hints\n" + describeText);

            auto toolErrorResult = assertMcpResult (parseMcpLine (lines, 7), 8);
            auto& toolErrorObject = asObject (toolErrorResult, "MCP endpoint error result");
            require ((bool) toolErrorObject.getProperty ("isError"),
                     "MCP endpoint errors should be tool results, not JSON-RPC errors");
            auto toolErrorText = asObject (toolErrorObject.getProperty ("content").getArray()->getReference (0),
                                           "MCP endpoint error content")
                                     .getProperty ("text")
                                     .toString();
            require (toolErrorText.contains ("locator_not_found") || toolErrorText.contains ("operation_timeout"),
                     "MCP endpoint error did not include structured compact context\n" + toolErrorText);
        }

        juce::var readSnapshot()
        {
            auto parsed = parseJsonOutput (runCli ({ "-s", sessionName, "snapshot", "--full", "--format", "json", "--depth", "12" }), "snapshot");
            asObject (parsed, "snapshot");
            return parsed;
        }

        juce::var readLocator (std::initializer_list<juce::String> locatorArgs)
        {
            auto args = makeArgs ({ "-s", sessionName, "locator", "--format", "json" });
            args.addArray (makeArgs (locatorArgs));

            auto parsed = parseJsonOutput (runCli (args), "locator");
            asObject (parsed, "locator");
            return parsed;
        }

        void waitForSession()
        {
            auto deadline = juce::Time::currentTimeMillis() + 15000;

            while (juce::Time::currentTimeMillis() < deadline && !threadShouldExit())
            {
                auto listOutput = runCli ({ "list" });

                if (listOutput.contains (juce::String (sessionName) + " "))
                    return;

                juce::Thread::sleep (250);
            }

            throw std::runtime_error ("Timed out waiting for automation_fixture to advertise an automation session");
        }

        void clickRef (const juce::String& ref)
        {
            runCli ({ "-s", sessionName, "click", ref });
        }

        void clickXYAtNode (const juce::var& node)
        {
            require (!node.isVoid(), "click-xy target node is missing");
            auto bounds = boundsOf (node);
            runCli ({ "-s",
                      sessionName,
                      "click-xy",
                      juce::String (bounds.getCentreX()),
                      juce::String (bounds.getCentreY()) });
        }

        void typeRef (const juce::String& ref, const juce::String& text)
        {
            runCli ({ "-s", sessionName, "type", ref, text });
        }

        void pressRef (const juce::String& ref, const juce::String& key)
        {
            runCli ({ "-s", sessionName, "press", key, "--ref", ref });
        }

        void dragRef (const juce::String& ref, int dx, int dy, int steps = 1)
        {
            runCli ({ "-s", sessionName, "drag", ref, "--dx", juce::String (dx), "--dy", juce::String (dy), "--steps", juce::String (steps) });
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationFixtureSelfTest)
    };
