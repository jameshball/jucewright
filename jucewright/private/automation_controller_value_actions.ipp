        juce::var check (juce::DynamicObject& params, bool shouldBeChecked)
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

            if (auto* button = dynamic_cast<juce::Button*> (target))
            {
                if (!button->isToggleable())
                    return error ("target_not_toggleable", "Target button is not toggleable.");

                button->setToggleState (shouldBeChecked, juce::sendNotification);
                return snapshotAfterAction();
            }

            auto* handler = target->getAccessibilityHandler();
            if (handler != nullptr && handler->getActions().contains (juce::AccessibilityActionType::toggle))
            {
                const auto state = handler->getCurrentState();
                if (state.isChecked() != shouldBeChecked)
                {
                    handler->getActions().invoke (juce::AccessibilityActionType::toggle);
                }

                return snapshotAfterAction();
            }

            return error ("target_not_toggleable", "Target component does not support check/uncheck.");
        }

        juce::var setChecked (juce::DynamicObject& params)
        {
            if (params.getProperty ("checked").isVoid())
                return error ("invalid_checked_state", "set_checked requires a checked boolean.");

            return check (params, (bool) params.getProperty ("checked"));
        }

        juce::var setValue (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto requestedValue = params.getProperty ("value");

            if (requestedValue.isVoid())
                return error ("invalid_value", "set_value requires a value.");

            auto resolution = resolveTarget (params, true, true);

            if (!resolution.error.isVoid())
                return resolution.error;

            auto* target = resolution.component;

            if (auto validationError = validateInputTarget (*target, params, false); !validationError.isVoid())
                return validationError;

            if (isTrial (params))
                return actionabilityResult (*target);

            if (auto* slider = dynamic_cast<juce::Slider*> (target))
            {
                double sliderValue = 0.0;

                if (!getFiniteDouble (requestedValue, sliderValue))
                    return error ("invalid_value", "set_value for a Slider requires a finite numeric value.");

                slider->setValue (sliderValue, juce::sendNotificationSync);
                return snapshotAfterAction();
            }

            if (auto* editor = dynamic_cast<juce::TextEditor*> (target))
            {
                editor->setText (requestedValue.toString(), juce::sendNotification);
                return snapshotAfterAction();
            }

            auto* handler = target->getAccessibilityHandler();
            if (handler != nullptr)
            {
                auto* valueInterface = handler->getValueInterface();
                if (valueInterface != nullptr && ! valueInterface->isReadOnly())
                {
                    valueInterface->setValueAsString (requestedValue.toString());
                    return snapshotAfterAction();
                }
            }

            return error ("target_no_value", "Target component does not support semantic set_value.");
        }

        juce::var selectOption (juce::DynamicObject& params)
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

            auto text = getString (params, "text", {});

            auto* handler = target->getAccessibilityHandler();
            if (handler != nullptr && handler->getRole() == juce::AccessibilityRole::menuItem)
            {
                if (auto* menuBar = dynamic_cast<juce::MenuBarComponent*> (target->getParentComponent()))
                    return selectMenuBarItem (*menuBar, params, text, handler->getTitle());
            }

            if (auto* menuBar = dynamic_cast<juce::MenuBarComponent*> (target))
                return selectMenuBarItem (*menuBar, params, text, {});

            auto* combo = dynamic_cast<juce::ComboBox*> (target);

            if (combo == nullptr)
            {
                if (auto* listBox = dynamic_cast<juce::ListBox*> (target))
                    return selectListBoxRow (*listBox, params, text);

                return error ("target_not_selectable", "Target component is not a ComboBox, ListBox, or MenuBarComponent.");
            }

            if (text.isNotEmpty())
            {
                for (int i = 0; i < combo->getNumItems(); ++i)
                {
                    if (normalizeForLocator (combo->getItemText (i)) == normalizeForLocator (text))
                    {
                        combo->setSelectedItemIndex (i, juce::sendNotificationSync);
                        return snapshotAfterAction();
                    }
                }

                return error ("option_not_found", "ComboBox option not found: " + text);
            }

            if (!params.getProperty ("index").isVoid())
            {
                const auto index = (int) params.getProperty ("index");

                if (index < 0 || index >= combo->getNumItems())
                    return error ("option_not_found", "ComboBox index is out of range: " + juce::String (index));

                combo->setSelectedItemIndex (index, juce::sendNotificationSync);
                return snapshotAfterAction();
            }

            if (!params.getProperty ("id").isVoid())
            {
                const auto id = (int) params.getProperty ("id");

                if (combo->indexOfItemId (id) < 0)
                    return error ("option_not_found", "ComboBox id is not present: " + juce::String (id));

                combo->setSelectedId (id, juce::sendNotificationSync);
                return snapshotAfterAction();
            }

            return error ("invalid_option", "select_option requires text, index, or id.");
        }

        juce::var selectMenuBarItem (juce::MenuBarComponent& menuBar,
                                     juce::DynamicObject& params,
                                     const juce::String& text,
                                     const juce::String& topLevelMenuFilter)
        {
            auto* model = menuBar.getModel();

            if (model == nullptr)
                return error ("target_not_selectable", "MenuBarComponent has no model.");

            auto menuNames = model->getMenuBarNames();
            int flatIndex = 0;
            const auto exact = (bool) params.getProperty ("exact");
            const auto followUpMenuItem = getString (params, "menuItem", {});

            for (int menuIndex = 0; menuIndex < menuNames.size(); ++menuIndex)
            {
                const auto menuName = menuNames[menuIndex];

                if (topLevelMenuFilter.isNotEmpty() && ! matchesString (menuName, topLevelMenuFilter, true))
                    continue;

                auto menu = model->getMenuForIndex (menuIndex, menuName);
                juce::PopupMenu::MenuItemIterator iterator (menu, true);

                while (iterator.next())
                {
                    auto& item = iterator.getItem();
                    const auto selectable = ! item.isSeparator && ! item.isSectionHeader;

                    if (!selectable)
                        continue;

                    auto matched = false;

                    if (text.isNotEmpty())
                        matched = matchesString (item.text, text, exact);
                    else if (!params.getProperty ("index").isVoid())
                        matched = flatIndex == (int) params.getProperty ("index");
                    else if (!params.getProperty ("id").isVoid())
                        matched = item.itemID == (int) params.getProperty ("id");

                    if (matched)
                    {
                        if (! item.isEnabled)
                            return error ("option_not_available", "Menu item is disabled: " + item.text);

                        if (item.action)
                            item.action();
                        else if (item.itemID != 0)
                            model->menuItemSelected (item.itemID, menuIndex);

                        if (followUpMenuItem.isNotEmpty())
                            return selectVisibleMenuItem (params, followUpMenuItem);

                        return snapshotAfterAction();
                    }

                    ++flatIndex;
                }
            }

            return error ("option_not_found", "Menu item not found: " + text);
        }

        juce::var selectVisibleMenuItem (juce::DynamicObject& params, const juce::String& menuItem)
        {
            auto* menuLocator = new juce::DynamicObject();
            menuLocator->setProperty ("class", "juce::PopupMenu::HelperClasses::ItemComponent");
            menuLocator->setProperty ("name", menuItem);

            if (! params.getProperty ("exact").isVoid())
                menuLocator->setProperty ("exact", params.getProperty ("exact"));

            juce::DynamicObject menuParams;
            menuParams.setProperty ("locator", juce::var (menuLocator));
            menuParams.setProperty ("force", true);

            const auto deadline = juce::Time::currentTimeMillis() + juce::jlimit (0, 30000, getInt (params, "timeoutMs", 5000));
            juce::var lastResult;

            do
            {
                auto item = resolveTarget (menuParams, true, true);

                if (item.error.isVoid())
                {
                    if (auto* coordinateRoot = coordinateRootFor (*item.component))
                    {
                        const auto rootBounds = getRootBounds (*item.component);
                        synthesizeClickAt (*coordinateRoot, rootBounds.getCentre());
                    }
                    else
                    {
                        synthesizeComponentClick (*item.component, juce::ModifierKeys(), 1, targetCentreLocal (*item.component));
                    }

                    waitForVisibleMenuWork (150);
                    return snapshotAfterAction();
                }

                lastResult = item.error;

                if (auto* errorObject = item.error.getDynamicObject())
                {
                    const auto code = errorObject->getProperty ("__error").toString();

                    if (code != "locator_not_found" && code != "stale_ref")
                        return item.error;
                }

                waitForVisibleMenuWork (25);
            } while (juce::Time::currentTimeMillis() < deadline && ! threadShouldExit());

            juce::ignoreUnused (lastResult);
            return error ("option_not_found", "Popup menu item not found: " + menuItem);
        }

        static void waitForVisibleMenuWork (int milliseconds)
        {
        #if JUCE_MODAL_LOOPS_PERMITTED
            juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
        #else
            juce::Thread::sleep (milliseconds);
        #endif
        }

        juce::var selectListBoxRow (juce::ListBox& listBox, juce::DynamicObject& params, const juce::String& text)
        {
            int row = -1;

            if (text.isNotEmpty())
            {
                if (auto* model = listBox.getListBoxModel())
                {
                    for (int i = 0; i < model->getNumRows(); ++i)
                    {
                        if (matchesString (model->getNameForRow (i), text, (bool) params.getProperty ("exact")))
                        {
                            row = i;
                            break;
                        }
                    }
                }
            }
            else if (!params.getProperty ("index").isVoid())
            {
                row = (int) params.getProperty ("index");
            }
            else if (!params.getProperty ("id").isVoid())
            {
                row = (int) params.getProperty ("id");
            }

            if (row < 0)
                return error ("option_not_found", "ListBox option not found: " + text);

            if (auto* model = listBox.getListBoxModel())
            {
                if (row >= model->getNumRows())
                    return error ("option_not_found", "ListBox row is out of range: " + juce::String (row));
            }

            listBox.selectRow (row, false, true);
            return snapshotAfterAction();
        }

        juce::var selectTab (juce::DynamicObject& params)
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

            auto* tabs = dynamic_cast<juce::TabbedComponent*> (target);

            if (tabs == nullptr)
                return error ("target_not_tabbed_component", "Target component is not a TabbedComponent.");

            if (!params.getProperty ("index").isVoid())
            {
                tabs->setCurrentTabIndex ((int) params.getProperty ("index"));
                return snapshotAfterAction();
            }

            auto name = getString (params, "name", {});
            auto tabNames = tabs->getTabNames();

            for (int i = 0; i < tabNames.size(); ++i)
            {
                if (normalizeForLocator (tabNames[i]) == normalizeForLocator (name))
                {
                    tabs->setCurrentTabIndex (i);
                    return snapshotAfterAction();
                }
            }

            return error ("tab_not_found", "TabbedComponent tab not found: " + name);
        }

        juce::var pressKey (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto key = parseKey (getString (params, "key", {}));
            juce::Component* target = nullptr;

            if (hasTargetSelector (params))
            {
                auto resolution = resolveTarget (params, true, true);

                if (!resolution.error.isVoid())
                    return resolution.error;

                target = resolution.component;
            }

            if (target != nullptr)
            {
                if (auto validationError = validateInputTarget (*target, params); !validationError.isVoid())
                    return validationError;

                if (isTrial (params))
                    return actionabilityResult (*target);

                target->grabKeyboardFocus();
            }

            if (target != nullptr && target->keyPressed (key.keyPress))
                return snapshotAfterAction();

            if (target != nullptr)
                for (auto* parent = target->getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
                    if (parent->keyPressed (key.keyPress))
                        return snapshotAfterAction();

            if (auto* peer = getRootPeer())
                peer->handleKeyPress (key.keyPress);

            return snapshotAfterAction();
        }

        juce::var keyDown (juce::DynamicObject& params)
        {
            return keyUpOrDown (params, true);
        }

        juce::var keyUp (juce::DynamicObject& params)
        {
            return keyUpOrDown (params, false);
        }

        juce::var keyUpOrDown (juce::DynamicObject& params, bool isDown)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto key = parseKey (getString (params, "key", {}));
            juce::Component* target = nullptr;

            if (hasTargetSelector (params))
            {
                auto resolution = resolveTarget (params, true, true);

                if (!resolution.error.isVoid())
                    return resolution.error;

                target = resolution.component;
            }

            if (target != nullptr)
            {
                if (auto validationError = validateInputTarget (*target, params); !validationError.isVoid())
                    return validationError;

                if (isTrial (params))
                    return actionabilityResult (*target);

                target->grabKeyboardFocus();

                if (isDown && target->keyPressed (key.keyPress))
                    return snapshotAfterAction();
            }

            if (auto* peer = getRootPeer())
            {
                peer->handleKeyUpOrDown (isDown);

                if (isDown)
                    peer->handleKeyPress (key.keyPress);
            }

            return snapshotAfterAction();
        }

        juce::var drag (juce::DynamicObject& params)
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

            auto* coordinateRoot = coordinateRootFor (*target);
            if (coordinateRoot == nullptr)
                return error ("invalid_target", "Target has no coordinate root.");

            juce::Point<float> localStart;
            if (auto positionError = localClickPoint (*target, params, localStart); ! positionError.isVoid())
                return positionError;

            auto start = coordinateRoot->getLocalPoint (target, localStart.roundToInt());
            auto end = start.translated (getInt (params, "dx", 0), getInt (params, "dy", 0));

            synthesizeDragOn (*target, start, end, getDragSteps (params));

            return snapshotAfterAction();
        }

        juce::var dragTo (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto source = resolveTarget (params, true, true);

            if (!source.error.isVoid())
                return source.error;

            auto* sourceComponent = source.component;

            if (auto validationError = validateInputTarget (*sourceComponent, params); !validationError.isVoid())
                return validationError;

            juce::DynamicObject targetParams;
            const auto targetRef = getString (params, "targetRef", {});
            auto targetLocator = params.getProperty ("targetLocator");

            if (targetRef.isEmpty() && !targetLocator.isObject())
                return error ("invalid_locator", "drag_to requires targetRef or targetLocator.");

            if (targetRef.isNotEmpty())
                targetParams.setProperty ("ref", targetRef);

            if (targetLocator.isObject())
                targetParams.setProperty ("locator", targetLocator);

            targetParams.setProperty ("force", params.getProperty ("force"));
            auto target = resolveTarget (targetParams, true, true);

            if (!target.error.isVoid())
                return target.error;

            auto* targetComponent = target.component;

            if (auto validationError = validateInputTarget (*targetComponent, params); !validationError.isVoid())
                return validationError;

            if (coordinateRootFor (*sourceComponent) != coordinateRootFor (*targetComponent))
                return error ("unsupported_cross_window_drag", "drag_to requires source and target to be in the same automation window.");

            if (isTrial (params))
                return object ({ { "source", actionabilityResult (*sourceComponent) },
                                 { "target", actionabilityResult (*targetComponent) } });

            synthesizeDragOn (*sourceComponent,
                              getRootBounds (*sourceComponent).getCentre(),
                              getRootBounds (*targetComponent).getCentre(),
                              getDragSteps (params));

            return snapshotAfterAction();
        }

        juce::var drop (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto target = resolveTarget (params, true, true);

            if (!target.error.isVoid())
                return target.error;

            auto* targetComponent = target.component;

            if (auto validationError = validateInputTarget (*targetComponent, params); !validationError.isVoid())
                return validationError;

            auto* dropTarget = dynamic_cast<juce::DragAndDropTarget*> (targetComponent);

            if (dropTarget == nullptr)
                return error ("invalid_drop_target", "Target component is not a juce::DragAndDropTarget.");

            auto description = params.getProperty ("description");

            if (description.isVoid())
                return error ("invalid_drop_description", "drop requires a description.");

            juce::Component* sourceComponent = targetComponent;
            juce::DynamicObject sourceParams;
            const auto sourceRef = getString (params, "sourceRef", {});
            auto sourceLocator = params.getProperty ("sourceLocator");

            if (sourceRef.isNotEmpty())
                sourceParams.setProperty ("ref", sourceRef);

            if (sourceLocator.isObject())
                sourceParams.setProperty ("locator", sourceLocator);

            if (sourceRef.isNotEmpty() || sourceLocator.isObject())
            {
                auto source = resolveTarget (sourceParams, true, true);

                if (!source.error.isVoid())
                    return source.error;

                sourceComponent = source.component;
            }

            juce::Point<float> localPoint;

            if (auto pointError = localClickPoint (*targetComponent, params, localPoint); !pointError.isVoid())
                return pointError;

            juce::DragAndDropTarget::SourceDetails details (description, sourceComponent, localPoint.roundToInt());

            if (!dropTarget->isInterestedInDragSource (details))
                return error ("drop_target_not_interested", "Target is not interested in this drag description.");

            if (isTrial (params))
                return actionabilityResult (*targetComponent);

            dropTarget->itemDragEnter (details);
            dropTarget->itemDragMove (details);
            dropTarget->itemDropped (details);

            return snapshotAfterAction();
        }

        juce::var dropFiles (juce::DynamicObject& params)
        {
            if (!options.allowInput)
                return error ("input_disabled", "Automation input is disabled for this session.");

            auto files = stringArrayFromVar (params.getProperty ("files"));

            if (files.isEmpty())
            {
                const auto file = getString (params, "file", {});

                if (file.isNotEmpty())
                    files.add (file);
            }

            files.removeEmptyStrings();

            if (files.isEmpty())
                return error ("invalid_file_drop", "drop_files requires a file or files array.");

            juce::Component* targetComponent = nullptr;

            if (hasTargetSelector (params))
            {
                auto target = resolveTarget (params, true, true);

                if (!target.error.isVoid())
                    return target.error;

                targetComponent = findFileDropTargetAtOrAbove (*target.component, files);
            }
            else if (root != nullptr)
            {
                targetComponent = findInterestedFileDropTarget (*root.getComponent(), files);
            }

            if (targetComponent == nullptr)
                return error ("invalid_file_drop_target", "No juce::FileDragAndDropTarget was interested in these files.");

            if (auto validationError = validateInputTarget (*targetComponent, params, false); !validationError.isVoid())
                return validationError;

            auto* dropTarget = dynamic_cast<juce::FileDragAndDropTarget*> (targetComponent);
            jassert (dropTarget != nullptr);

            juce::Point<float> localPoint;

            if (auto pointError = localClickPoint (*targetComponent, params, localPoint); !pointError.isVoid())
                return pointError;

            const auto point = localPoint.roundToInt();

            if (isTrial (params))
                return object ({ { "files", stringArrayToVar (files) },
                                 { "target", actionabilityResult (*targetComponent) } });

            dropTarget->fileDragEnter (files, point.x, point.y);
            dropTarget->fileDragMove (files, point.x, point.y);
            dropTarget->filesDropped (files, point.x, point.y);
            dropTarget->fileDragExit (files);

            return snapshotAfterAction();
        }

        static juce::Component* findFileDropTargetAtOrAbove (juce::Component& component, const juce::StringArray& files)
        {
            for (auto* current = &component; current != nullptr; current = current->getParentComponent())
            {
                auto* dropTarget = dynamic_cast<juce::FileDragAndDropTarget*> (current);

                if (dropTarget != nullptr && dropTarget->isInterestedInFileDrag (files))
                    return current;
            }

            return nullptr;
        }

        static juce::Component* findInterestedFileDropTarget (juce::Component& component, const juce::StringArray& files)
        {
            if (auto* target = findFileDropTargetAtOrAbove (component, files))
                return target;

            for (int i = 0; i < component.getNumChildComponents(); ++i)
            {
                if (auto* child = component.getChildComponent (i))
                    if (auto* target = findInterestedFileDropTarget (*child, files))
                        return target;
            }

            return nullptr;
        }

        juce::var resizeWindow (juce::DynamicObject& params)
        {
            if (!options.allowMutation)
                return error ("mutation_disabled", "Automation window resizing is disabled for this session.");

            auto targetId = getString (params, "target", "root");

            if (targetId.isEmpty())
                targetId = "root";

            const auto width = getInt (params, "w", 0);
            const auto height = getInt (params, "h", 0);

            if (width <= 0 || height <= 0)
                return error ("invalid_size", "resize_window requires positive w and h values.");

            for (auto& window : automationWindows())
            {
                if (window.id != targetId)
                    continue;

                auto* component = window.component.getComponent();

                if (component == nullptr)
                    return error ("stale_window", "Automation window is no longer attached: " + targetId);

                auto* resizeTarget = component;

                if (window.attachedRoot)
                {
                    if (auto* topLevel = component->getTopLevelComponent())
                        resizeTarget = topLevel;
                }

                resizeTarget->setSize (width, height);
                return snapshotAfterAction();
            }

            return error ("window_not_found", "No automation window named: " + targetId);
        }

        juce::var wait (juce::DynamicObject& params)
        {
            juce::ignoreUnused (params);
            return snapshotAfterAction();
        }

        juce::var snapshotAfterAction()
        {
            juce::DynamicObject params;
            params.setProperty ("format", "text");
            params.setProperty ("depth", 8);
            return snapshot (params);
        }
