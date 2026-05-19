    class CallbackTabbedComponent : public juce::TabbedComponent
    {
    public:
        explicit CallbackTabbedComponent (juce::TabbedButtonBar::Orientation orientation)
            : juce::TabbedComponent (orientation)
        {
        }

        void currentTabChanged (int index, const juce::String& name) override
        {
            if (onCurrentTabChanged)
                onCurrentTabChanged (index, name);
        }

        std::function<void (int, const juce::String&)> onCurrentTabChanged;
    };

    class ControlsPage : public juce::Component
    {
    public:
        class OptionsModel : public juce::ListBoxModel
        {
        public:
            int getNumRows() override { return options.size(); }

            juce::String getNameForRow (int rowNumber) override
            {
                return juce::isPositiveAndBelow (rowNumber, options.size()) ? options[rowNumber] : juce::String();
            }

            void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
            {
                g.fillAll (rowIsSelected ? juce::Colours::steelblue : juce::Colours::darkgrey);
                g.setColour (juce::Colours::white);
                g.drawText (getNameForRow (rowNumber), 8, 0, width - 16, height, juce::Justification::centredLeft);
            }

            void selectedRowsChanged (int lastRowSelected) override
            {
                if (onSelected && juce::isPositiveAndBelow (lastRowSelected, options.size()))
                    onSelected (options[lastRowSelected]);
            }

            juce::StringArray options { "Red", "Green", "Blue" };
            std::function<void (const juce::String&)> onSelected;
        };

        ControlsPage()
        {
            setName ("Controls Page");

            title.setText ("Controls Page", juce::dontSendNotification);
            title.setName ("controls.title");
            title.setComponentID ("controls.title");
            addAndMakeVisible (title);

            goEditor.setButtonText ("Go Editor");
            goEditor.setName ("nav.editor");
            goEditor.setComponentID ("nav.editor");
            addAndMakeVisible (goEditor);

            toggle.setButtonText ("Power Toggle");
            toggle.setName ("controls.power");
            toggle.setComponentID ("controls.power");
            addAndMakeVisible (toggle);

            slider.setName ("controls.slider");
            slider.setComponentID ("controls.slider");
            slider.setRange (0.0, 100.0, 1.0);
            slider.setValue (25.0);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 24);
            addAndMakeVisible (slider);

            combo.setName ("controls.combo");
            combo.setComponentID ("controls.combo");
            combo.addItem ("Alpha", 1);
            combo.addItem ("Beta", 2);
            combo.addItem ("Gamma", 3);
            combo.setSelectedId (1);
            addAndMakeVisible (combo);

            duplicateA.setButtonText ("Duplicate");
            duplicateA.setName ("controls.duplicateA");
            duplicateA.setComponentID ("controls.duplicateA");
            addAndMakeVisible (duplicateA);

            duplicateB.setButtonText ("Duplicate");
            duplicateB.setName ("controls.duplicateB");
            duplicateB.setComponentID ("controls.duplicateB");
            addAndMakeVisible (duplicateB);

            disabled.setButtonText ("Disabled Action");
            disabled.setName ("controls.disabled");
            disabled.setComponentID ("controls.disabled");
            disabled.setEnabled (false);
            addAndMakeVisible (disabled);

            adjacentButton.setButtonText ("Nearby Action");
            adjacentButton.setComponentID ("controls.nearbyAction");
            addAndMakeVisible (adjacentButton);

            adjacentEditor.setComponentID ("controls.adjacentEditor");
            addAndMakeVisible (adjacentEditor);

            compositeToggleHost.setName ("Composite MIDI");
            compositeToggleHost.setComponentID ("controls.compositeToggleHost");
            addAndMakeVisible (compositeToggleHost);

            compositeToggle.setName ("SwitchButton");
            compositeToggle.setComponentID ("controls.compositeToggle");
            compositeToggleHost.addAndMakeVisible (compositeToggle);

            optionList.setName ("controls.optionList");
            optionList.setComponentID ("controls.optionList");
            optionList.setModel (&optionsModel);
            optionList.setRowHeight (24);
            addAndMakeVisible (optionList);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (16);
            title.setBounds (area.removeFromTop (28));
            goEditor.setBounds (area.removeFromTop (34).removeFromLeft (140));
            area.removeFromTop (10);
            toggle.setBounds (area.removeFromTop (30).removeFromLeft (180));
            area.removeFromTop (10);
            slider.setBounds (area.removeFromTop (36).removeFromLeft (360));
            area.removeFromTop (10);
            combo.setBounds (area.removeFromTop (30).removeFromLeft (180));
            area.removeFromTop (10);
            duplicateA.setBounds (area.removeFromTop (30).removeFromLeft (140));
            duplicateB.setBounds (area.removeFromTop (30).removeFromLeft (140));
            area.removeFromTop (10);
            auto actionRow = area.removeFromTop (30);
            disabled.setBounds (actionRow.removeFromLeft (160));
            actionRow.removeFromLeft (8);
            adjacentButton.setBounds (actionRow.removeFromLeft (130));
            actionRow.removeFromLeft (8);
            adjacentEditor.setBounds (actionRow.removeFromLeft (130));
            actionRow.removeFromLeft (8);
            compositeToggleHost.setBounds (actionRow.removeFromLeft (130));
            compositeToggle.setBounds (compositeToggleHost.getLocalBounds().removeFromLeft (30));
            area.removeFromTop (10);
            optionList.setBounds (area.removeFromTop (82).removeFromLeft (180));
        }

        juce::TextButton goEditor;
        juce::ToggleButton toggle;
        juce::Slider slider;
        juce::ComboBox combo;
        juce::TextButton duplicateA;
        juce::TextButton duplicateB;
        juce::TextButton disabled;
        juce::TextButton adjacentButton;
        juce::TextEditor adjacentEditor;
        juce::Component compositeToggleHost;
        juce::ToggleButton compositeToggle;
        OptionsModel optionsModel;
        juce::ListBox optionList;

    private:
        juce::Label title;
    };

    class DragBox : public juce::Component
    {
    public:
        DragBox()
        {
            setName ("advanced.dragBox");
            setComponentID ("advanced.dragBox");
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::darkslategrey);
            g.setColour (juce::Colours::white);
            g.drawFittedText ("Drag Box", getLocalBounds(), juce::Justification::centred, 1);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            dragStartBounds = getBounds();
            dragEvents = 0;
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            ++dragEvents;
            setBounds (dragStartBounds.translated (event.getDistanceFromDragStartX(), event.getDistanceFromDragStartY()));

            if (onDragged)
                onDragged (getBounds(), dragEvents);
        }

        std::function<void (juce::Rectangle<int>, int)> onDragged;

    private:
        juce::Rectangle<int> dragStartBounds;
        int dragEvents = 0;
    };

    class InputProbe : public juce::Component
    {
    public:
        InputProbe()
        {
            setName ("advanced.inputProbe");
            setComponentID ("advanced.inputProbe");
            setWantsKeyboardFocus (true);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::darkblue);
            g.setColour (juce::Colours::white);
            g.drawFittedText ("Input Probe", getLocalBounds(), juce::Justification::centred, 1);
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (event.mods.isRightButtonDown() && onRightClick)
                onRightClick();
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (onDoubleClick)
                onDoubleClick();
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (onKeyPressed)
                onKeyPressed (describeKey (key));

            return true;
        }

        std::function<void()> onDoubleClick;
        std::function<void()> onRightClick;
        std::function<void (const juce::String&)> onKeyPressed;

    private:
        static juce::String describeKey (const juce::KeyPress& key)
        {
            juce::String result;

            if (key.getModifiers().isCtrlDown())
                result << "Ctrl+";

            if (key.getModifiers().isCommandDown() && !key.getModifiers().isCtrlDown())
                result << "Meta+";

            if (key.getModifiers().isAltDown())
                result << "Alt+";

            if (key.getModifiers().isShiftDown())
                result << "Shift+";

            auto code = key.getKeyCode();

            if (code > 0 && code < 128)
                result << juce::String::charToString ((juce::juce_wchar) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code));
            else
                result << juce::String (code);

            return result;
        }
    };

    class EditorPage : public juce::Component
    {
    public:
        EditorPage()
        {
            setName ("Editor Page");

            title.setText ("Editor Page", juce::dontSendNotification);
            title.setName ("editor.title");
            title.setComponentID ("editor.title");
            addAndMakeVisible (title);

            text.setName ("editor.text");
            text.setComponentID ("editor.text");
            text.setTextToShowWhenEmpty ("Type here", juce::Colours::grey);
            addAndMakeVisible (text);

            apply.setButtonText ("Apply Text");
            apply.setName ("editor.apply");
            apply.setComponentID ("editor.apply");
            addAndMakeVisible (apply);

            goAdvanced.setButtonText ("Go Advanced");
            goAdvanced.setName ("nav.advanced");
            goAdvanced.setComponentID ("nav.advanced");
            addAndMakeVisible (goAdvanced);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (16);
            title.setBounds (area.removeFromTop (28));
            text.setBounds (area.removeFromTop (36).removeFromLeft (300));
            area.removeFromTop (10);
            apply.setBounds (area.removeFromTop (34).removeFromLeft (140));
            area.removeFromTop (10);
            goAdvanced.setBounds (area.removeFromTop (34).removeFromLeft (140));
        }

        juce::TextEditor text;
        juce::TextButton apply;
        juce::TextButton goAdvanced;

    private:
        juce::Label title;
    };

    class AdvancedPage : public juce::Component
    {
    public:
        AdvancedPage()
        {
            setName ("Advanced Page");

            nestedTabs.setName ("advanced.tabs");
            nestedTabs.setComponentID ("advanced.tabs");
            nestedTabs.addTab ("Metrics", juce::Colours::darkgrey, &metrics, false);
            nestedTabs.addTab ("Actions", juce::Colours::darkgrey, &actions, false);
            addAndMakeVisible (nestedTabs);

            metrics.setName ("Metrics Page");
            metricLabel.setText ("Metrics Ready", juce::dontSendNotification);
            metricLabel.setName ("advanced.metrics.label");
            metricLabel.setComponentID ("advanced.metrics.label");
            metrics.addAndMakeVisible (metricLabel);

            goActions.setButtonText ("Go Actions");
            goActions.setName ("advanced.goActions");
            goActions.setComponentID ("advanced.goActions");
            metrics.addAndMakeVisible (goActions);

            actions.setName ("Actions Page");
            reset.setButtonText ("Reset All");
            reset.setName ("advanced.reset");
            reset.setComponentID ("advanced.reset");
            actions.addAndMakeVisible (reset);

            actions.addAndMakeVisible (dragBox);
            actions.addAndMakeVisible (inputProbe);

            goActions.onClick = [this] {
                nestedTabs.setCurrentTabIndex (1);
            };

            nestedTabs.onCurrentTabChanged = [this] (int index, const juce::String& name) {
                juce::ignoreUnused (index);
                if (onNestedTabChanged)
                    onNestedTabChanged (name);
            };
        }

        void resized() override
        {
            nestedTabs.setBounds (getLocalBounds().reduced (16));
            auto metricsArea = metrics.getLocalBounds().reduced (16);
            metricLabel.setBounds (metricsArea.removeFromTop (30));
            metricsArea.removeFromTop (10);
            goActions.setBounds (metricsArea.removeFromTop (34).removeFromLeft (140));

            reset.setBounds (actions.getLocalBounds().reduced (16).removeFromTop (34).removeFromLeft (140));
            resetDragBoxBounds();
            inputProbe.setBounds (360, 24, 120, 42);
        }

        void resetDragBoxBounds()
        {
            dragBox.setBounds (240, 24, 100, 42);
        }

        CallbackTabbedComponent nestedTabs { juce::TabbedButtonBar::TabsAtTop };
        juce::TextButton goActions;
        juce::TextButton reset;
        DragBox dragBox;
        InputProbe inputProbe;
        std::function<void (const juce::String&)> onNestedTabChanged;

    private:
        juce::Component metrics;
        juce::Component actions;
        juce::Label metricLabel;
    };

    class AutomationRoot : public juce::Component
    {
    public:
        AutomationRoot()
        {
            setName ("Automation Fixture Root");

            status.setName ("fixture.status");
            status.setComponentID ("fixture.status");
            status.setText ("Status: Controls", juce::dontSendNotification);
            addAndMakeVisible (status);

            tabs.setName ("fixture.tabs");
            tabs.setComponentID ("fixture.tabs");
            tabs.addTab ("Controls", juce::Colours::lightgrey, &controls, false);
            tabs.addTab ("Editor", juce::Colours::lightgrey, &editor, false);
            tabs.addTab ("Advanced", juce::Colours::lightgrey, &advanced, false);
            addAndMakeVisible (tabs);

            controls.goEditor.onClick = [this] {
                tabs.setCurrentTabIndex (1);
                setStatus ("Status: Editor");
            };

            controls.toggle.onClick = [this] {
                setStatus (controls.toggle.getToggleState() ? "Status: Power On" : "Status: Power Off");
            };

            controls.slider.onValueChange = [this] {
                setStatus ("Status: Slider " + juce::String (juce::roundToInt (controls.slider.getValue())));
            };

            controls.optionsModel.onSelected = [this] (const juce::String& option) {
                setStatus ("Status: List " + option);
            };

            editor.apply.onClick = [this] {
                setStatus ("Status: Applied " + editor.text.getText());
            };

            editor.goAdvanced.onClick = [this] {
                tabs.setCurrentTabIndex (2);
                setStatus ("Status: Advanced");
            };

            advanced.reset.onClick = [this] {
                editor.text.clear();
                controls.toggle.setToggleState (false, juce::dontSendNotification);
                controls.slider.setValue (25.0, juce::dontSendNotification);
                advanced.resetDragBoxBounds();
                setStatus ("Status: Reset");
            };

            advanced.onNestedTabChanged = [this] (const juce::String& name) {
                setStatus ("Status: Nested " + name);
            };

            advanced.dragBox.onDragged = [this] (juce::Rectangle<int> bounds, int dragEvents) {
                setStatus ("Status: DragBox " + juce::String (bounds.getX()) + "," + juce::String (bounds.getY()) + " steps=" + juce::String (dragEvents));
            };

            advanced.inputProbe.onDoubleClick = [this] {
                setStatus ("Status: DoubleClick");
            };

            advanced.inputProbe.onRightClick = [this] {
                setStatus ("Status: RightClick");
            };

            advanced.inputProbe.onKeyPressed = [this] (const juce::String& key) {
                setStatus ("Status: Key " + key);
            };

            tabs.onCurrentTabChanged = [this] (int index, const juce::String& name) {
                juce::ignoreUnused (index);
                setStatus ("Status: " + name);
            };
        }

        void resized() override
        {
            auto area = getLocalBounds();
            status.setBounds (area.removeFromBottom (34).reduced (12, 4));
            tabs.setBounds (area);
        }

    private:
        void setStatus (const juce::String& text)
        {
            status.setText (text, juce::dontSendNotification);
        }

        juce::Label status;
        CallbackTabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
        ControlsPage controls;
        EditorPage editor;
        AdvancedPage advanced;
    };
