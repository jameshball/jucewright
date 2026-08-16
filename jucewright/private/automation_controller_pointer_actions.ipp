        juce::var click (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto resolution = resolveTarget (params, true, true);

            if (!resolution.error.isVoid())
                return resolution.error;

            const auto buttonName = getString (params, "button", "left");
            juce::ModifierKeys buttonModifiers;

            if (!parseMouseButton (buttonName, buttonModifiers))
                return error ("invalid_button", "Unknown mouse button: " + buttonName);

            const auto clickCount = getInt (params, "clickCount", 1);

            if (clickCount < 1)
                return error ("invalid_click_count", "clickCount must be at least 1.");

            auto* target = resolution.component;
            const auto canUseSemanticClick = buttonName == "left" && clickCount == 1 && !hasClickPosition (params);
            auto validationError = validateInputTarget (*target, params);
            if (!validationError.isVoid())
                return validationError;

            if (isTrial (params))
                return actionabilityResult (*target);

            juce::Point<float> localPoint;

            if (auto pointError = localClickPoint (*target, params, localPoint); !pointError.isVoid())
                return pointError;

            if (auto* button = dynamic_cast<juce::Button*> (target))
            {
                if (canUseSemanticClick)
                {
                    button->triggerClick();
                    return snapshotAfterAction();
                }
            }

            if (canUseSemanticClick)
            {
                if (auto* menuBar = dynamic_cast<juce::MenuBarComponent*> (target->getParentComponent()))
                {
                    const auto menuIndex = menuBar->getIndexOfChildComponent (target);

                    if (menuIndex >= 0)
                    {
                        menuBar->showMenu (menuIndex);
                        return snapshotAfterAction();
                    }
                }
            }

            if (canUseSemanticClick)
            {
                if (invokeAccessibleClick (*target))
                {
                    return snapshotAfterAction();
                }

            }

            synthesizeComponentClick (*target, buttonModifiers, clickCount, localPoint);
            return snapshotAfterAction();
        }

        juce::var doubleClick (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto resolution = resolveTarget (params, true, true);

            if (!resolution.error.isVoid())
                return resolution.error;

            auto* target = resolution.component;

            if (auto validationError = validateInputTarget (*target, params); !validationError.isVoid())
                return validationError;

            if (isTrial (params))
                return actionabilityResult (*target);

            if (auto* button = dynamic_cast<juce::Button*> (target))
            {
                button->triggerClick();
                button->triggerClick();
            }
            else
            {
                synthesizeComponentClick (*target, juce::ModifierKeys(), 2, targetCentreLocal (*target));
            }

            return snapshotAfterAction();
        }

        juce::var rightClick (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto resolution = resolveTarget (params, true, true);

            if (!resolution.error.isVoid())
                return resolution.error;

            auto* target = resolution.component;

            if (auto validationError = validateInputTarget (*target, params); !validationError.isVoid())
                return validationError;

            if (isTrial (params))
                return actionabilityResult (*target);

            synthesizeComponentClick (*target, juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier), 1, targetCentreLocal (*target));

            const auto menuItem = getString (params, "menuItem", {});
            if (menuItem.isNotEmpty())
            {
                auto* menuLocator = new juce::DynamicObject();
                menuLocator->setProperty ("role", "menuItem");
                menuLocator->setProperty ("name", menuItem);
                menuLocator->setProperty ("exact", true);

                juce::DynamicObject menuParams;
                menuParams.setProperty ("locator", juce::var (menuLocator));
                menuParams.setProperty ("force", true);

                auto item = resolveTarget (menuParams, true, true);

                if (!item.error.isVoid())
                    return item.error;

                synthesizeComponentClick (*item.component, juce::ModifierKeys(), 1, targetCentreLocal (*item.component));
            }

            return snapshotAfterAction();
        }

        juce::var clickXY (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto* coordinateRoot = pointerCoordinateRoot (params);

            if (coordinateRoot == nullptr)
                return error ("window_not_found", "No automation window matched target: " + getString (params, "target", "root"));

            const auto rootPoint = juce::Point<int> { getInt (params, "x", 0), getInt (params, "y", 0) };

            if (coordinateRoot != nullptr)
            {
                if (auto* target = findComponentAt (*coordinateRoot, rootPoint))
                {
                    if (target->isEnabled())
                    {
                        if (auto* button = dynamic_cast<juce::Button*> (target))
                            button->triggerClick();
                        else
                            synthesizeClickAt (*coordinateRoot, rootPoint);
                    }
                }
                else
                {
                    synthesizeClickAt (*coordinateRoot, rootPoint);
                }
            }

            return snapshotAfterAction();
        }

        juce::var mouseMove (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto* coordinateRoot = pointerCoordinateRoot (params);

            if (coordinateRoot == nullptr)
                return error ("window_not_found", "No automation window matched target: " + getString (params, "target", "root"));

            sendPeerMouseEvent (*coordinateRoot, pointFromParams (params, "x", "y"), juce::ModifierKeys(), 0.0f);
            return snapshotAfterAction();
        }

        juce::var mouseButton (juce::DynamicObject& params, bool isDown)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto* coordinateRoot = pointerCoordinateRoot (params);

            if (coordinateRoot == nullptr)
                return error ("window_not_found", "No automation window matched target: " + getString (params, "target", "root"));

            sendPeerMouseEvent (*coordinateRoot,
                                pointFromParams (params, "x", "y"),
                                isDown ? juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier) : juce::ModifierKeys(),
                                isDown ? 1.0f : 0.0f);
            return snapshotAfterAction();
        }

        juce::var wheel (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto* coordinateRoot = pointerCoordinateRoot (params);

            if (coordinateRoot == nullptr)
                return error ("window_not_found", "No automation window matched target: " + getString (params, "target", "root"));

            if (auto* peer = coordinateRoot->getPeer())
            {
                juce::MouseWheelDetails details;
                details.deltaX = (float) params.getProperty ("deltaX");
                details.deltaY = (float) params.getProperty ("deltaY");
                details.isReversed = (bool) params.getProperty ("isReversed");
                details.isSmooth = true;

                peer->handleMouseWheel (juce::MouseInputSource::InputSourceType::mouse,
                                        peer->getComponent().getLocalPoint (coordinateRoot, pointFromParams (params, "x", "y")).toFloat(),
                                        juce::Time::currentTimeMillis(),
                                        details);
            }

            return snapshotAfterAction();
        }

        juce::var dragXY (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto* coordinateRoot = pointerCoordinateRoot (params);

            if (coordinateRoot == nullptr)
                return error ("window_not_found", "No automation window matched target: " + getString (params, "target", "root"));

            auto start = pointFromParams (params, "x", "y");
            auto end = pointFromParams (params, "toX", "toY");

            if (auto* target = findComponentAt (*coordinateRoot, start))
            {
                synthesizeDragOn (*target, start, end, getDragSteps (params));
                return snapshotAfterAction();
            }

            return error ("locator_not_found", "No component was found at the drag start point.");
        }

        juce::var typeText (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto resolution = resolveTarget (params, true, true);

            if (!resolution.error.isVoid())
                return resolution.error;

            auto* target = resolution.component;

            if (auto validationError = validateInputTarget (*target, params); !validationError.isVoid())
                return validationError;

            if (isTrial (params))
                return actionabilityResult (*target);

            target->grabKeyboardFocus();
            const auto text = getString (params, "text", {});

            if (auto* editor = dynamic_cast<juce::TextEditor*> (target))
            {
                editor->insertTextAtCaret (text);
            }
            else if (auto* codeEditor = dynamic_cast<juce::CodeEditorComponent*> (target))
            {
                codeEditor->insertTextAtCaret (text);
            }
            else if (auto* label = dynamic_cast<juce::Label*> (target))
            {
                label->setText (label->getText() + text, juce::sendNotification);
            }
            else if (auto* peer = getRootPeer())
            {
                for (int i = 0; i < text.length(); ++i)
                    peer->handleKeyPress (0, text[i]);
            }

            return snapshotAfterAction();
        }

        juce::var fill (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto resolution = resolveTarget (params, true, true);

            if (!resolution.error.isVoid())
                return resolution.error;

            auto* target = resolution.component;

            if (auto validationError = validateInputTarget (*target, params, false); !validationError.isVoid())
                return validationError;

            if (isTrial (params))
                return actionabilityResult (*target);

            const auto text = getString (params, "text", {});

            if (auto* editor = dynamic_cast<juce::TextEditor*> (target))
            {
                editor->setText (text, juce::sendNotification);
                return snapshotAfterAction();
            }

            if (auto* codeEditor = dynamic_cast<juce::CodeEditorComponent*> (target))
            {
                codeEditor->loadContent (text);
                return snapshotAfterAction();
            }

            if (auto* label = dynamic_cast<juce::Label*> (target))
            {
                label->setText (text, juce::sendNotification);
                return snapshotAfterAction();
            }

            return error ("target_not_editable", "Target component does not support semantic fill.");
        }

        juce::var clear (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto resolution = resolveTarget (params, true, true);

            if (!resolution.error.isVoid())
                return resolution.error;

            auto* target = resolution.component;

            if (auto validationError = validateInputTarget (*target, params, false); !validationError.isVoid())
                return validationError;

            if (isTrial (params))
                return actionabilityResult (*target);

            if (auto* editor = dynamic_cast<juce::TextEditor*> (target))
            {
                editor->clear();
                return snapshotAfterAction();
            }

            if (auto* codeEditor = dynamic_cast<juce::CodeEditorComponent*> (target))
            {
                codeEditor->loadContent ({});
                return snapshotAfterAction();
            }

            if (auto* label = dynamic_cast<juce::Label*> (target))
            {
                label->setText ({}, juce::sendNotification);
                return snapshotAfterAction();
            }

            return error ("target_not_editable", "Target component does not support semantic clear.");
        }
