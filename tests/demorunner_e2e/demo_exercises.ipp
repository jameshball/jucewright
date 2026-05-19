        void exerciseAdditionalDemoCoverage()
        {
            const DemoCase demos[] {
                { "GUI", "CodeEditorDemo.h",          "code-editor-demo.png", DemoCase::Exercise::codeEditor },
                { "GUI", "ComponentDemo.h",           "component-demo.png", DemoCase::Exercise::componentGrid },
                { "GUI", "ComponentTransformsDemo.h", "component-transforms-demo.png", DemoCase::Exercise::componentTransforms },
                { "GUI", "DialogsDemo.h",             "dialogs-demo.png", DemoCase::Exercise::dialogs },
                { "GUI", "GridDemo.h",                "grid-demo.png", DemoCase::Exercise::grid },
                { "GUI", "ImagesDemo.h",              "images-demo.png", DemoCase::Exercise::images },
                { "GUI", "FontsDemo.h",               "fonts-demo.png", DemoCase::Exercise::fonts },
                { "GUI", "WidgetsDemo.h",             "widgets-demo.png", DemoCase::Exercise::widgets },
                { "GUI", "MenusDemo.h",               "menus-demo.png", DemoCase::Exercise::menus },
                { "GUI", "WindowsDemo.h",             "windows-demo.png", DemoCase::Exercise::windows },
                { "GUI", "MDIDemo.h",                 "mdi-demo.png", DemoCase::Exercise::mdi },
                { "GUI", "PropertiesDemo.h",          "properties-demo.png", DemoCase::Exercise::properties },
                { "GUI", "KeyMappingsDemo.h",         "key-mappings-demo.png", DemoCase::Exercise::keyMappings },
                { "Audio", "AudioSettingsDemo.h",     "audio-settings-demo.png", DemoCase::Exercise::audioSettings },
                { "DSP", "GainDemo.h",                "gain-demo.png", DemoCase::Exercise::gain },
                { "Utilities", "ValueTreesDemo.h",    "value-trees-demo.png", DemoCase::Exercise::valueTrees },
                { "Utilities", "XMLandJSONDemo.h",    "xml-and-json-demo.png", DemoCase::Exercise::xmlAndJson },
                { "GUI", "OpenGLAppDemo.h",           "opengl-app-demo.png", DemoCase::Exercise::openGLApp },
                { "GUI", "OpenGLDemo.h",              "opengl-demo.png", DemoCase::Exercise::openGL },
                { "GUI", "OpenGLDemo2D.h",            "opengl-2d-demo.png", DemoCase::Exercise::openGL2D }
            };

            for (const auto& demo : demos)
            {
                selectDemoFromCategory (demo.category, demo.file);
                assertCodeTabLoads();
                captureScreenshot (demo.screenshot);
                exerciseDemo (demo);
            }
        }

        void exerciseDemo (const DemoCase& demo)
        {
            switch (demo.exercise)
            {
                case DemoCase::Exercise::codeEditor:           exerciseCodeEditorDemo(); break;
                case DemoCase::Exercise::componentGrid:        exerciseComponentDemo(); break;
                case DemoCase::Exercise::componentTransforms:  exerciseComponentTransformsDemo(); break;
                case DemoCase::Exercise::dialogs:              exerciseDialogsDemo(); break;
                case DemoCase::Exercise::grid:                 exerciseGridDemo(); break;
                case DemoCase::Exercise::images:               exerciseImagesDemo(); break;
                case DemoCase::Exercise::fonts:                exerciseFontsDemo(); break;
                case DemoCase::Exercise::audioSettings:        exerciseAudioSettingsDemo(); break;
                case DemoCase::Exercise::gain:                 exerciseGainDemo(); break;
                case DemoCase::Exercise::valueTrees:           exerciseValueTreesDemo(); break;
                case DemoCase::Exercise::xmlAndJson:           exerciseXmlAndJsonDemo(); break;
                case DemoCase::Exercise::openGLApp:            exerciseOpenGLAppDemo(); break;
                case DemoCase::Exercise::openGL:               exerciseOpenGLDemo(); break;
                case DemoCase::Exercise::widgets:              exerciseWidgetsDemo(); break;
                case DemoCase::Exercise::menus:                exerciseMenusDemo(); break;
                case DemoCase::Exercise::windows:              exerciseWindowsDemo(); break;
                case DemoCase::Exercise::mdi:                  exerciseMdiDemo(); break;
                case DemoCase::Exercise::properties:           exercisePropertiesDemo(); break;
                case DemoCase::Exercise::keyMappings:          exerciseKeyMappingsDemo(); break;
                case DemoCase::Exercise::openGL2D:             exerciseOpenGL2DDemo(); break;
            }
        }

        void exerciseCodeEditorDemo()
        {
            auto before = captureScreenshot ("code-editor-before-type.png");
            runCli ({ "-s", sessionName, "click", "--role", "editableText", "--nth", "0", "--force", "--timeout-ms", "3000" });

            for (int i = 0; i < 8; ++i)
                runCli ({ "-s", sessionName, "press", "return", "--role", "editableText", "--nth", "0", "--force" });

            auto after = captureScreenshot ("code-editor-after-type.png");
            assertScreenshotsDiffer (before, after, "CodeEditor keyboard editing", 4);
        }

        void exerciseComponentDemo()
        {
            auto before = captureScreenshot ("component-before-window-resize.png");
            auto lightBounds = boundsOf (firstLocatorMatch ({ "--class", "ToggleLightComponent", "--nth", "0", "--visible" },
                                                            "ComponentDemo light"));
            runCli ({ "-s", sessionName, "mouse-move", "830", "580" });
            runCli ({ "-s", sessionName, "mouse-move", juce::String (lightBounds.getCentreX()), juce::String (lightBounds.getCentreY()) });
            runCli ({ "-s", sessionName, "wait", "--ms", "150" });

            auto grid = firstLocatorMatch ({ "--class", "ToggleLightGridComponent", "--nth", "0", "--visible" },
                                           "ComponentDemo light grid");
            auto bounds = boundsOf (grid);
            auto windowBounds = rootWindowBounds();
            resizeRootWindow (juce::jmax (640, windowBounds.getWidth() - 120),
                              juce::jmax (520, windowBounds.getHeight() - 80));
            auto afterBounds = boundsOf (firstLocatorMatch ({ "--class", "ToggleLightGridComponent", "--nth", "0", "--visible" },
                                                            "ComponentDemo light grid after window resize"));
            require (afterBounds.getWidth() != bounds.getWidth() || afterBounds.getHeight() != bounds.getHeight(),
                     "ComponentDemo light-grid bounds did not respond to window resize");
            auto after = captureScreenshot ("component-after-window-resize.png");
            assertScreenshotsDiffer (before, after, "ComponentDemo window resize");
            resizeRootWindow (900, 650);
        }

        void exerciseComponentTransformsDemo()
        {
            auto before = captureScreenshot ("component-transforms-before-drag.png");
            auto dragger = firstLocatorMatch ({ "--class", "CornerDragger", "--nth", "0", "--visible" },
                                              "ComponentTransforms dragger");
            auto beforeBounds = boundsOf (dragger);
            runCli ({ "-s", sessionName, "drag", nodeRef (dragger), "--dx", "70", "--dy", "45", "--steps", "5" });
            auto afterBounds = boundsOf (firstLocatorMatch ({ "--class", "CornerDragger", "--nth", "0", "--visible" },
                                                           "ComponentTransforms moved dragger"));
            require (afterBounds.getCentre().getDistanceFrom (beforeBounds.getCentre()) > 20.0f,
                     "ComponentTransforms dragger did not move");
            auto after = captureScreenshot ("component-transforms-after-drag.png");
            assertScreenshotsDiffer (before, after, "ComponentTransforms drag");
        }

        void exerciseDialogsDemo()
        {
            runCli ({ "-s", sessionName, "uncheck", "--role", "toggleButton", "--name", "Use Native Windows", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "toggleButton", "--name", "Use Native Windows", "--exact", "--value", "false", "--timeout-ms", "3000" });
            captureScreenshot ("dialogs-before-alert-window.png");

            const auto initialWindowCount = windowCount();
            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Alert Window With Extra Components", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-text", "AlertWindow demo..", "--timeout-ms", "3000" });
            require (windowCount() > initialWindowCount, "Opening a non-native AlertWindow did not add an automation window");

            auto dialogId = secondaryWindowIdContaining ("AlertWindow demo");
            captureScreenshot ("dialogs-extra-components-window.png", { "--target", dialogId, "--source", "component" });

            runCli ({ "-s", sessionName, "fill", "--class", "juce::TextEditor", "--value", "enter some text here", "--exact", "--visible", "Automation dialog input" });
            runCli ({ "-s", sessionName, "wait-for-value", "--class", "juce::TextEditor", "--visible", "--value", "Automation dialog input", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "select-option", "--class", "juce::ComboBox", "--value", "option 1", "--exact", "--visible", "--text", "option 3" });
            runCli ({ "-s", sessionName, "wait-for-value", "--class", "juce::ComboBox", "--visible", "--value", "option 3", "--exact", "--timeout-ms", "3000" });
            captureScreenshot ("dialogs-extra-components-filled.png", { "--target", dialogId, "--source", "component" });

            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "OK", "--exact", "--visible", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-text", "Automation dialog input", "--timeout-ms", "3000" });

            dialogId = secondaryWindowIdContaining ("Alert Box");
            captureScreenshot ("dialogs-result-window.png", { "--target", dialogId, "--source", "component" });
            auto resultOk = firstLocatorMatch ({ "--role", "button", "--name", "OK", "--exact", "--visible" },
                                               "Dialogs result OK button");
            auto okBounds = boundsOf (resultOk);
            runCli ({ "-s", sessionName, "click-xy", juce::String (okBounds.getCentreX()), juce::String (okBounds.getCentreY()), "--target", dialogId });
            runCli ({ "-s", sessionName, "wait-for-locator", "--role", "button", "--name", "Alert Window With Extra Components", "--exact", "--visible", "--timeout-ms", "3000" });
            require (windowCount() == initialWindowCount, "Dismissing non-native AlertWindows did not restore the original window count");
        }

        void exerciseGridDemo()
        {
            auto before = captureScreenshot ("grid-before-window-resize.png");
            auto windowBounds = rootWindowBounds();
            resizeRootWindow (windowBounds.getWidth() + 140, windowBounds.getHeight() + 100);
            auto after = captureScreenshot ("grid-after-window-resize.png");
            assertScreenshotsDiffer (before, after, "GridDemo responsive resize");
            resizeRootWindow (900, 650);
        }

        void exerciseImagesDemo()
        {
            auto before = captureScreenshot ("images-before-resize-bar-drag.png");
            auto resizer = firstLocatorMatch ({ "--class", "StretchableLayoutResizerBar", "--nth", "0", "--visible" },
                                              "ImagesDemo resizer");
            runCli ({ "-s", sessionName, "drag", nodeRef (resizer), "--dx", "0", "--dy", "120", "--steps", "5" });
            auto after = captureScreenshot ("images-after-resize-bar-drag.png");
            assertScreenshotsDiffer (before, after, "ImagesDemo resizer drag");
        }

        void exerciseFontsDemo()
        {
            runCli ({ "-s", sessionName, "fill", "--role", "editableText", "--nth", "0", "Automation fonts demo" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "editableText", "--nth", "0", "--value", "Automation fonts demo", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "check", "--role", "toggleButton", "--name", "Bold", "--exact" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "toggleButton", "--name", "Bold", "--exact", "--value", "true", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "check", "--role", "toggleButton", "--name", "Italic", "--exact" });
            runCli ({ "-s", sessionName, "set-value", "--role", "slider", "--nth", "0", "32" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "slider", "--nth", "0", "--value", "32", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "select-option", "--role", "comboBox", "--nth", "1", "--text", "Right" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "comboBox", "--nth", "1", "--value", "Right", "--timeout-ms", "3000" });
            captureScreenshot ("fonts-after-controls.png");
        }

        void exerciseWidgetsDemo()
        {
            runCli ({ "-s", sessionName, "wait-for-locator", "--class", "DemoTabbedComponent", "--visible", "--timeout-ms", "3000" });

            runCli ({ "-s", sessionName, "check", "--role", "radioButton", "--name", "Radio Button #3", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "radioButton", "--name", "Radio Button #3", "--exact", "--value", "true", "--timeout-ms", "3000" });
            captureScreenshot ("widgets-buttons-after-radio.png");

            selectDemoLocalTab ("Sliders");
            runCli ({ "-s", sessionName, "set-value", "--role", "slider", "--nth", "0", "25" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "slider", "--nth", "0", "--value", "25", "--timeout-ms", "3000" });
            captureScreenshot ("widgets-sliders-before-drag.png");
            auto slider = firstLocatorMatch ({ "--role", "slider", "--nth", "2", "--visible" },
                                             "Widgets horizontal slider");
            const auto beforeSliderValue = asObject (slider, "Widgets horizontal slider").getProperty ("value").toString().getDoubleValue();
            runCli ({ "-s", sessionName, "drag", nodeRef (slider), "--dx", "90", "--dy", "0", "--steps", "5" });
            auto draggedSlider = firstLocatorMatch ({ "--role", "slider", "--nth", "2", "--visible" },
                                                    "Widgets dragged horizontal slider");
            const auto afterSliderValue = asObject (draggedSlider, "Widgets dragged horizontal slider").getProperty ("value").toString().getDoubleValue();
            require (std::abs (afterSliderValue - beforeSliderValue) > 0.001, "Widgets horizontal slider drag did not change its semantic value");
            captureScreenshot ("widgets-sliders-after-drag.png");

            selectDemoLocalTab ("Toolbars");
            runCli ({ "-s", sessionName, "set-value", "--role", "slider", "--nth", "0", "90" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "slider", "--nth", "0", "--value", "90", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Vertical/Horizontal", "--exact", "--timeout-ms", "3000" });
            captureScreenshot ("widgets-toolbar-after-orientation.png");

            selectDemoLocalTab ("Misc");
            runCli ({ "-s", sessionName, "fill", "--role", "editableText", "--value", "Single-line text box", "--exact", "Automation widgets misc" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "editableText", "--nth", "0", "--value", "Automation widgets misc", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "select-option", "--role", "comboBox", "--value", "combo box item 1", "--exact", "--text", "combo box item 4" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "comboBox", "--value", "combo box item 4", "--exact", "--timeout-ms", "3000" });
            captureScreenshot ("widgets-misc-after-edit.png");

            selectDemoLocalTab ("Menus");
            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Short", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-locator", "--role", "menuItem", "--name", "Single Item", "--exact", "--visible", "--timeout-ms", "3000" });
            captureScreenshot ("widgets-popup-menu.png");
            runCli ({ "-s", sessionName, "press", "escape", "--timeout-ms", "3000" });

            selectDemoLocalTab ("Tables");
            runCli ({ "-s", sessionName, "select-option", "--class", "TableListBox", "--nth", "0", "--index", "2", "--timeout-ms", "3000" });
            auto table = findSnapshotNode (readSnapshot (8), "Widgets table", [] (juce::DynamicObject& node) {
                return isVisible (node) && hasClass (node, "TableListBox");
            });
            require (nodeInt (table, "selectedRow") == 2, "Widgets table did not select row 2");
            captureScreenshot ("widgets-table-after-select.png");

            selectDemoLocalTab ("Drag & Drop");
            auto dragBefore = captureScreenshot ("widgets-dragdrop-before.png");
            auto sourceList = firstLocatorMatch ({ "--class", "ListBox", "--nth", "0", "--visible" },
                                                 "Widgets drag source list");
            auto target = firstLocatorMatch ({ "--class", "DragAndDropDemoTarget", "--nth", "0", "--visible" },
                                             "Widgets drag target");
            auto sourceBounds = boundsOf (sourceList);
            auto targetBounds = boundsOf (target);
            runCli ({ "-s", sessionName, "drag-xy",
                      juce::String (sourceBounds.getX() + 35),
                      juce::String (sourceBounds.getY() + 18),
                      juce::String (targetBounds.getCentreX()),
                      juce::String (targetBounds.getCentreY()),
                      "--steps", "8" });
            auto dragAfter = captureScreenshot ("widgets-dragdrop-after.png");
            assertScreenshotsDiffer (dragBefore, dragAfter, "Widgets drag-and-drop", 4);
        }

        void exerciseMenusDemo()
        {
            runCli ({ "-s", sessionName, "wait-for-text", "Menu Position", "--timeout-ms", "3000" });

            captureScreenshot ("menus-before-popup.png");
            clickVisibleText ("Outer Colour");
            runCli ({ "-s", sessionName, "wait", "--ms", "250" });
            captureScreenshot ("menus-after-outer-colour-click.png");
            runCli ({ "-s", sessionName, "press", "escape", "--timeout-ms", "3000" });

            clickVisibleText ("Inner Colour");
            runCli ({ "-s", sessionName, "wait", "--ms", "250" });
            captureScreenshot ("menus-after-inner-colour-click.png");
            runCli ({ "-s", sessionName, "press", "escape", "--timeout-ms", "3000" });
        }

        void exerciseWindowsDemo()
        {
            const auto initialWindowCount = windowCount();
            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Show Windows", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-text", "Dialog Windows can be used", "--timeout-ms", "3000" });
            require (windowCount() >= initialWindowCount + 4, "WindowsDemo did not open the expected secondary windows");
            captureScreenshot ("windows-after-show-all.png");

            auto alertId = secondaryWindowIdContaining ("Alert Window");
            captureScreenshot ("windows-alert-window.png", { "--target", alertId, "--source", "component" });
            runCli ({ "-s", sessionName, "fill", "--class", "juce::TextEditor", "--value", "Text editor", "--exact", "--visible", "Automation window text" });
            runCli ({ "-s", sessionName, "wait-for-value", "--class", "juce::TextEditor", "--nth", "0", "--value", "Automation window text", "--exact", "--visible", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "select-option", "--class", "juce::ComboBox", "--value", "Combo box", "--exact", "--visible", "--text", "Item 3" });
            runCli ({ "-s", sessionName, "wait-for-value", "--class", "juce::ComboBox", "--value", "Item 3", "--exact", "--visible", "--timeout-ms", "3000" });
            captureScreenshot ("windows-alert-filled.png", { "--target", alertId, "--source", "component" });
            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Button 2", "--exact", "--visible", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-text", "Dismissed the Alert Window using Button 2", "--timeout-ms", "3000" });

            auto dialogId = secondaryWindowIdContaining ("Dialog Window");
            captureScreenshot ("windows-dialog-window.png", { "--target", dialogId, "--source", "component" });
            runCli ({ "-s", sessionName, "press", "escape", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait", "--ms", "500" });

            if (windowCount() > initialWindowCount)
            {
                runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Close Windows", "--exact", "--force", "--timeout-ms", "3000" });
                runCli ({ "-s", sessionName, "wait", "--ms", "750" });
            }

            require (windowCount() == initialWindowCount, "WindowsDemo Close Windows did not restore the original window count");
            captureScreenshot ("windows-after-close-all.png");
        }

        void exerciseMdiDemo()
        {
            runCli ({ "-s", sessionName, "wait-for-text", "Show with tabs", "--timeout-ms", "3000" });
            auto panel = visibleNodeByClass (readSnapshot (8), "DemoMultiDocumentPanel");
            require (nodeInt (panel, "documentCount") >= 1, "MDIDemo did not expose initial document count");
            require (nodeString (panel, "layoutMode") == "floating", "MDIDemo did not start in floating window mode");

            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Create a new note", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Create a new note", "--exact", "--timeout-ms", "3000" });
            panel = visibleNodeByClass (readSnapshot (8), "DemoMultiDocumentPanel");
            require (nodeInt (panel, "documentCount") >= 3, "MDIDemo did not create additional notes");
            captureScreenshot ("mdi-after-create-notes.png");

            runCli ({ "-s", sessionName, "check", "--role", "toggleButton", "--name", "Show with tabs", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "toggleButton", "--name", "Show with tabs", "--exact", "--value", "true", "--timeout-ms", "3000" });
            panel = visibleNodeByClass (readSnapshot (8), "DemoMultiDocumentPanel");
            require (nodeString (panel, "layoutMode") == "tabs", "MDIDemo did not switch to tabbed layout");
            captureScreenshot ("mdi-tabbed-layout.png");

            runCli ({ "-s", sessionName, "fill", "--role", "editableText", "--nth", "0", "Automation MDI note" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "editableText", "--nth", "0", "--value", "Automation MDI note", "--timeout-ms", "3000" });
            captureScreenshot ("mdi-after-edit-note.png");

            const auto countBeforeClose = nodeInt (panel, "documentCount");
            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Close active document", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait", "--ms", "500" });
            panel = visibleNodeByClass (readSnapshot (8), "DemoMultiDocumentPanel");
            require (nodeInt (panel, "documentCount") == countBeforeClose - 1, "MDIDemo did not close the active document");
            captureScreenshot ("mdi-after-close-active.png");
        }

        void exercisePropertiesDemo()
        {
            runCli ({ "-s", sessionName, "wait-for-text", "Text Editors", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait", "--ms", "500" });
            runCli ({ "-s", sessionName, "fill", "--role", "editableText", "--value", "This is a single-line Text Property", "--exact", "Automation property value" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "editableText", "--nth", "0", "--value", "Automation property value", "--exact", "--timeout-ms", "3000" });
            captureScreenshot ("properties-text-after-fill.png");

            runCli ({ "-s", sessionName, "select-option", "--role", "comboBox", "--nth", "0", "--text", "Item 5" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "comboBox", "--nth", "0", "--value", "Item 5", "--timeout-ms", "3000" });
            captureScreenshot ("properties-choice-after-select.png");

            runCli ({ "-s", sessionName, "check", "--role", "toggleButton", "--nth", "0", "--force", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "toggleButton", "--nth", "0", "--value", "true", "--timeout-ms", "3000" });
            captureScreenshot ("properties-toggle-after-check.png");

            runCli ({ "-s", sessionName, "click-xy", "35", "235" });
            runCli ({ "-s", sessionName, "wait-for-locator", "--role", "slider", "--visible", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "set-value", "--role", "slider", "--nth", "0", "--force", "64" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "slider", "--nth", "0", "--value", "64", "--timeout-ms", "3000" });
            captureScreenshot ("properties-slider-after-set.png");
        }

        void exerciseKeyMappingsDemo()
        {
            runCli ({ "-s", sessionName, "wait-for-locator", "--class", "KeyPressTarget", "--visible", "--timeout-ms", "3000" });
            auto target = visibleNodeByClass (readSnapshot (8), "KeyPressTarget");
            auto button = findNode (target, [] (juce::DynamicObject& node) {
                return isVisible (node) && hasClass (node, "TextButton");
            });
            require (! button.isVoid(), "Could not find KeyMappings target button");
            captureScreenshot ("key-mappings-before-keys.png");

            runCli ({ "-s", sessionName, "click", "--class", "KeyPressTarget", "--force", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "press", "right", "--class", "KeyPressTarget", "--force", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "press", "down", "--class", "KeyPressTarget", "--force", "--timeout-ms", "3000" });
            target = visibleNodeByClass (readSnapshot (8), "KeyPressTarget");
            button = findNode (target, [] (juce::DynamicObject& node) {
                return isVisible (node) && hasClass (node, "TextButton");
            });
            require (! button.isVoid(), "Could not find KeyMappings target button after key presses");

            runCli ({ "-s", sessionName, "press", "shift+right", "--class", "KeyPressTarget", "--force", "--timeout-ms", "3000" });
            captureScreenshot ("key-mappings-after-keys.png");
        }

        void exerciseAudioSettingsDemo()
        {
            auto before = captureScreenshot ("audio-settings-before-toggle.png");

            if (locatorCount ({ "--role", "toggleButton", "--name", "IAC Driver Bus 1", "--visible" }, "AudioSettings MIDI toggle buttons") > 0)
            {
                auto toggle = firstLocatorMatch ({ "--role", "toggleButton", "--name", "IAC Driver Bus 1", "--visible" },
                                                 "AudioSettings MIDI toggle");
                const auto wasChecked = (bool) asObject (toggle, "AudioSettings MIDI toggle").getProperty ("checked");

                if (wasChecked)
                    runCli ({ "-s", sessionName, "uncheck", "--role", "toggleButton", "--name", "IAC Driver Bus 1", "--visible", "--force", "--timeout-ms", "3000" });
                else
                    runCli ({ "-s", sessionName, "check", "--role", "toggleButton", "--name", "IAC Driver Bus 1", "--visible", "--force", "--timeout-ms", "3000" });

                auto updated = firstLocatorMatch ({ "--role", "toggleButton", "--name", "IAC Driver Bus 1", "--visible" },
                                                  "AudioSettings MIDI toggle after click");
                const auto isChecked = (bool) asObject (updated, "AudioSettings MIDI toggle after click").getProperty ("checked");
                require (isChecked != wasChecked, "AudioSettings MIDI toggle did not change state");

                auto after = captureScreenshot ("audio-settings-after-toggle.png");
                assertScreenshotsDiffer (before, after, "AudioSettings MIDI toggle", 3);

                if (wasChecked)
                    runCli ({ "-s", sessionName, "check", "--role", "toggleButton", "--name", "IAC Driver Bus 1", "--visible", "--force", "--timeout-ms", "3000" });
                else
                    runCli ({ "-s", sessionName, "uncheck", "--role", "toggleButton", "--name", "IAC Driver Bus 1", "--visible", "--force", "--timeout-ms", "3000" });
            }
            else
            {
                auto diagnostics = firstLocatorMatch ({ "--class", "juce::TextEditor", "--nth", "0", "--visible" },
                                                      "AudioSettings diagnostics editor");
                auto bounds = boundsOf (diagnostics);
                runCli ({ "-s", sessionName, "click-xy", juce::String (bounds.getCentreX()), juce::String (bounds.getCentreY()) });
                captureScreenshot ("audio-settings-after-diagnostics-click.png");
            }
        }

        void exerciseGainDemo()
        {
            runCli ({ "-s", sessionName, "set-value", "--role", "slider", "--nth", "0", "6" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "slider", "--nth", "0", "--value", "6", "--timeout-ms", "3000" });
            auto before = captureScreenshot ("gain-before-slider-drag.png");
            auto slider = firstLocatorMatch ({ "--role", "slider", "--nth", "0", "--visible" },
                                             "GainDemo slider");
            runCli ({ "-s", sessionName, "drag", nodeRef (slider), "--dx", "-120", "--dy", "0", "--steps", "5" });
            auto after = captureScreenshot ("gain-after-slider-drag.png");
            assertScreenshotsDiffer (before, after, "GainDemo slider drag");
        }

        void exerciseValueTreesDemo()
        {
            auto before = captureScreenshot ("value-trees-before-delete.png");
            auto beforeCount = locatorCount ({ "--role", "treeItem", "--visible" }, "ValueTrees tree items");
            require (beforeCount > 4, "ValueTrees demo did not expose enough tree items");

            runCli ({ "-s", sessionName, "click", "--role", "treeItem", "--nth", "4", "--visible", "--force", "--timeout-ms", "3000" });
            require (locatorCount ({ "--role", "treeItem", "--selected", "--visible" }, "ValueTrees selected tree item") == 1,
                     "ValueTrees tree item click did not select exactly one visible item");

            runCli ({ "-s", sessionName, "press", "backspace", "--role", "treeItem", "--selected", "--visible", "--force" });
            auto afterDeleteCount = locatorCount ({ "--role", "treeItem", "--visible" }, "ValueTrees tree items after delete");
            require (afterDeleteCount < beforeCount, "ValueTrees delete did not reduce the visible tree item count");
            auto afterDelete = captureScreenshot ("value-trees-after-delete.png");
            assertScreenshotsDiffer (before, afterDelete, "ValueTrees delete", 4);

            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Undo", "--exact", "--timeout-ms", "3000" });
            auto afterUndoCount = locatorCount ({ "--role", "treeItem", "--visible" }, "ValueTrees tree items after undo");
            require (afterUndoCount >= beforeCount, "ValueTrees undo did not restore the deleted tree item");
            captureScreenshot ("value-trees-after-undo.png");
        }

        void exerciseXmlAndJsonDemo()
        {
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "comboBox", "--nth", "0", "--value", "XML", "--timeout-ms", "3000" });
            auto before = captureScreenshot ("xml-json-before-type-select.png");
            runCli ({ "-s", sessionName, "select-option", "--role", "comboBox", "--nth", "0", "--text", "JSON" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "comboBox", "--nth", "0", "--value", "JSON", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "click", "--role", "editableText", "--nth", "0", "--force", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "press", "backspace", "--role", "editableText", "--nth", "0", "--force" });
            auto after = captureScreenshot ("xml-json-after-json-edit.png");
            assertScreenshotsDiffer (before, after, "XML/JSON select and edit");
        }

        void exerciseOpenGLDemo()
        {
            runCli ({ "-s", sessionName, "wait-for-text", "Shader Preset:", "--timeout-ms", "7000" });
            runCli ({ "-s", sessionName, "check", "--role", "toggleButton", "--name", "Draw 2D graphics in background", "--exact", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "toggleButton", "--name", "Draw 2D graphics in background", "--exact", "--value", "true", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait", "--ms", "1000" });

            auto before = captureScreenshot ("opengl-demo-composited-before-slider.png", { "--source", "component" });
            assertScreenshotHasVariation (before, "OpenGL composited component screenshot", 18, 15);

            auto slider = firstLocatorMatch ({ "--role", "slider", "--nth", "0", "--visible" },
                                             "OpenGL demo zoom slider");
            const auto beforeValue = asObject (slider, "OpenGL demo zoom slider").getProperty ("value").toString().getDoubleValue();
            runCli ({ "-s", sessionName, "drag", nodeRef (slider), "--dx", "80", "--dy", "0", "--steps", "6", "--timeout-ms", "3000" });
            slider = firstLocatorMatch ({ "--role", "slider", "--nth", "0", "--visible" },
                                        "OpenGL demo zoom slider after drag");
            const auto afterValue = asObject (slider, "OpenGL demo zoom slider after drag").getProperty ("value").toString().getDoubleValue();
            require (std::abs (afterValue - beforeValue) > 0.001, "OpenGL demo slider drag did not change its semantic value");

            auto after = captureScreenshot ("opengl-demo-composited-after-slider.png", { "--source", "component" });
            assertScreenshotHasVariation (after, "OpenGL composited screenshot after slider drag", 18, 15);
            assertScreenshotsDiffer (before, after, "OpenGL composited slider interaction", 8);

            auto scene = captureScreenshot ("opengl-demo-composited-scene-clip.png",
                                            { "--source", "component",
                                              "--clip-x", "170",
                                              "--clip-y", "145",
                                              "--clip-w", "500",
                                              "--clip-h", "270" });
            assertScreenshotHasVariation (scene, "OpenGL composited clipped scene screenshot", 18, 15);
        }

        void exerciseOpenGLAppDemo()
        {
            runCli ({ "-s", sessionName, "wait-for-locator", "--class", "OpenGLAppDemo", "--visible", "--timeout-ms", "7000" });
            runCli ({ "-s", sessionName, "wait", "--ms", "1000" });

            auto rootShot = captureScreenshot ("opengl-app-root-composited.png", { "--source", "component" });
            assertScreenshotHasVariation (rootShot, "OpenGLApp root composited screenshot", 18, 15);

            auto componentShot = captureScreenshot ("opengl-app-component-composited.png",
                                                    { "--source", "component",
                                                      "--class", "OpenGLAppDemo",
                                                      "--nth", "0",
                                                      "--visible" });
            assertScreenshotHasVariation (componentShot, "OpenGLApp component composited screenshot", 6, 15);
        }

        void exerciseOpenGL2DDemo()
        {
            runCli ({ "-s", sessionName, "wait-for-text", "Shader Preset:", "--timeout-ms", "7000" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "comboBox", "--nth", "0", "--value", "Simple Gradient", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait", "--ms", "1000" });

            auto before = captureScreenshot ("opengl-2d-composited-before.png", { "--source", "component" });
            assertScreenshotHasVariation (before, "OpenGL2D composited screenshot before", 18, 15);

            runCli ({ "-s", sessionName, "select-option", "--role", "comboBox", "--nth", "0", "--text", "Solid Colour" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "comboBox", "--nth", "0", "--value", "Solid Colour", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "click", "--class", "CodeEditorComponent", "--nth", "0", "--force", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "press", "backspace", "--class", "CodeEditorComponent", "--nth", "0", "--force", "--timeout-ms", "3000" });
            runCli ({ "-s", sessionName, "wait", "--ms", "1000" });

            auto after = captureScreenshot ("opengl-2d-composited-after.png", { "--source", "component" });
            assertScreenshotHasVariation (after, "OpenGL2D composited screenshot after", 18, 15);
            assertScreenshotsDiffer (before, after, "OpenGL2D shader selection/edit", 8);
        }

        void exerciseAccessibilityDemo()
        {
            auto snapshot = readSnapshot (8);
            auto demoTabs = visibleNodeByClassAndName (snapshot, "juce::TabbedComponent", "Demo tabs");
            auto tabNames = asObject (demoTabs, "Demo tabs").getProperty ("tabNames");
            require (tabNames.isArray() && tabNames.getArray()->size() >= 2, "Accessibility demo tabs did not expose tab names");

            runCli ({ "-s", sessionName, "click", "--role", "button", "--name", "Press me!", "--exact" });

            runCli ({ "-s", sessionName, "check", "--role", "radioButton", "--name", "Button 2", "--exact" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "radioButton", "--name", "Button 2", "--exact", "--value", "true", "--timeout-ms", "3000" });

            runCli ({ "-s", sessionName, "set-value", "--role", "slider", "--nth", "0", "42" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "slider", "--nth", "0", "--value", "42", "--timeout-ms", "3000" });

            demoTabs = visibleNodeByClassAndName (readSnapshot (8), "juce::TabbedComponent", "Demo tabs");
            runCli ({ "-s", sessionName, "select-tab", nodeRef (demoTabs), "--name", "Custom Widget" });
            runCli ({ "-s", sessionName, "wait-for-text", "Description", "--timeout-ms", "3000" });
            captureScreenshot ("accessibility-custom-widget.png");

            runCli ({ "-s", sessionName, "fill", "--role", "editableText", "--value", "Custom", "--exact", "Automation Custom" });
            require ((int) asObject (readLocator ({ "--role", "editableText", "--value", "Automation Custom", "--exact" }),
                                     "Automation Custom locator").getProperty ("count") >= 1,
                     "Accessibility custom widget title editor did not update");
        }

        void exerciseFlexBoxDemo()
        {
            runCli ({ "-s", sessionName, "check", "--role", "radioButton", "--name", "column", "--exact" });
            runCli ({ "-s", sessionName, "wait-for-value", "--role", "radioButton", "--name", "column", "--exact", "--value", "true", "--timeout-ms", "3000" });

            runCli ({ "-s", sessionName, "fill", "--role", "editableText", "--value", "1", "--exact", "--nth", "0", "2" });
            require ((int) asObject (readLocator ({ "--role", "editableText", "--value", "2", "--exact" }),
                                     "FlexBox editor locator").getProperty ("count") >= 1,
                     "FlexBox flex-grow editor did not update");

            runCli ({ "-s", sessionName, "select-option", "--role", "comboBox", "--value", "stretch", "--exact", "--nth", "0", "--text", "center" });
            require ((int) asObject (readLocator ({ "--role", "comboBox", "--value", "center", "--exact" }),
                                     "FlexBox combo locator").getProperty ("count") >= 1,
                     "FlexBox align-self combo did not update");
        }
