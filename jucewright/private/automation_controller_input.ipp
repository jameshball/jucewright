        juce::var validateInputTarget (juce::Component& target, juce::DynamicObject& params, bool requirePointerEvents = true) const
        {
            if ((bool) params.getProperty ("force"))
                return {};

            if (!target.isShowing())
                return error ("target_not_showing", "Target component is not showing.");

            if (!target.isEnabled())
                return error ("target_disabled", "Target component is disabled.");

            scrollAncestorViewportsToReveal (target);

            if (getRootBounds (target).isEmpty())
                return error ("target_empty_bounds", "Target component has empty bounds.");

            if (requirePointerEvents && !receivesEvents (target))
                return error ("target_not_receiving_events", "Target component does not receive pointer events at its center.");

            return {};
        }

        void scrollAncestorViewportsToReveal (juce::Component& target) const
        {
            for (auto* parent = target.getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
            {
                auto* viewport = dynamic_cast<juce::Viewport*> (parent);

                if (viewport == nullptr)
                    continue;

                auto* viewedComponent = viewport->getViewedComponent();

                if (viewedComponent == nullptr)
                    continue;

                if (viewedComponent != &target && !viewedComponent->isParentOf (&target))
                    continue;

                const auto targetBounds = viewedComponent->getLocalArea (&target, target.getLocalBounds());
                const auto viewArea = viewport->getViewArea();

                if (targetBounds.isEmpty() || viewArea.isEmpty())
                    continue;

                auto nextX = viewArea.getX();
                auto nextY = viewArea.getY();
                constexpr int margin = 4;

                if (targetBounds.getX() < viewArea.getX())
                    nextX = targetBounds.getX() - margin;
                else if (targetBounds.getRight() > viewArea.getRight())
                    nextX = targetBounds.getRight() - viewArea.getWidth() + margin;

                if (targetBounds.getY() < viewArea.getY())
                    nextY = targetBounds.getY() - margin;
                else if (targetBounds.getBottom() > viewArea.getBottom())
                    nextY = targetBounds.getBottom() - viewArea.getHeight() + margin;

                nextX = juce::jlimit (0, juce::jmax (0, viewedComponent->getWidth() - viewArea.getWidth()), nextX);
                nextY = juce::jlimit (0, juce::jmax (0, viewedComponent->getHeight() - viewArea.getHeight()), nextY);

                if (nextX != viewArea.getX() || nextY != viewArea.getY())
                    viewport->setViewPosition (nextX, nextY);
            }
        }

        static bool isTrial (juce::DynamicObject& params)
        {
            return (bool) params.getProperty ("trial");
        }

        juce::var actionabilityResult (juce::Component& target) const
        {
            const auto bounds = getRootBounds (target);
            return object ({ { "actionability", object ({ { "attached", true },
                                                          { "visible", target.isShowing() && !bounds.isEmpty() },
                                                          { "enabled", target.isEnabled() },
                                                          { "nonEmptyBounds", !bounds.isEmpty() },
                                                          { "receivesEvents", receivesEvents (target) } }) } });
        }

        bool receivesEvents (juce::Component& target) const
        {
            auto* coordinateRoot = coordinateRootFor (target);

            if (coordinateRoot == nullptr)
                return false;

            auto rootBounds = getRootBounds (target);

            if (rootBounds.isEmpty())
                return false;

            auto* found = findComponentAt (*coordinateRoot, rootBounds.getCentre());

            if (found == &target || (found != nullptr && target.isParentOf(found))) {
                return true;
            }

            // Menu items are accessibility proxies; the menu bar receives their mouse events.
            auto* handler = target.getAccessibilityHandler();
            return found == target.getParentComponent()
                && dynamic_cast<juce::MenuBarComponent*>(found) != nullptr
                && handler != nullptr && handler->getRole() == juce::AccessibilityRole::menuItem;
        }

        bool invokeAccessibleClick (juce::Component& target) const
        {
            auto* handler = target.getAccessibilityHandler();

            if (handler == nullptr)
                return false;

            auto& actions = handler->getActions();

            if (handler->getRole() == juce::AccessibilityRole::menuItem)
            {
                if (auto* menuBar = dynamic_cast<juce::MenuBarComponent*> (target.getParentComponent()))
                {
                    if (auto* model = menuBar->getModel())
                    {
                        auto menuNames = model->getMenuBarNames();
                        const auto title = handler->getTitle();
                        const auto index = menuNames.indexOf (title);

                        if (index >= 0)
                        {
                            menuBar->showMenu (index);
                            return true;
                        }
                    }
                }
            }

            if (handler->getRole() == juce::AccessibilityRole::treeItem && !handler->getCurrentState().isSelected())
                actions.invoke (juce::AccessibilityActionType::toggle);

            if (actions.contains (juce::AccessibilityActionType::press))
            {
                actions.invoke (juce::AccessibilityActionType::press);
                return true;
            }

            if (actions.contains (juce::AccessibilityActionType::toggle))
            {
                actions.invoke (juce::AccessibilityActionType::toggle);
                return true;
            }

            return false;
        }

        static bool parseMouseButton (const juce::String& buttonName, juce::ModifierKeys& modifiers)
        {
            const auto normalized = buttonName.trim().toLowerCase();

            if (normalized == "left" || normalized.isEmpty())
            {
                modifiers = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
                return true;
            }

            if (normalized == "right")
            {
                modifiers = juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier);
                return true;
            }

            if (normalized == "middle")
            {
                modifiers = juce::ModifierKeys (juce::ModifierKeys::middleButtonModifier);
                return true;
            }

            return false;
        }

        static bool hasClickPosition (juce::DynamicObject& params)
        {
            return params.getProperty ("position").isObject()
                   || (!params.getProperty ("positionX").isVoid() && !params.getProperty ("positionY").isVoid());
        }

        juce::Point<float> targetCentreLocal (juce::Component& target) const
        {
            return target.getLocalBounds().getCentre().toFloat();
        }

        juce::var localClickPoint (juce::Component& target, juce::DynamicObject& params, juce::Point<float>& point) const
        {
            if (!hasClickPosition (params))
            {
                point = targetCentreLocal (target);
                return {};
            }

            auto position = params.getProperty ("position");

            if (auto* positionObject = position.getDynamicObject())
            {
                point = { (float) positionObject->getProperty ("x"), (float) positionObject->getProperty ("y") };
            }
            else
            {
                point = { (float) params.getProperty ("positionX"), (float) params.getProperty ("positionY") };
            }

            if (!target.getLocalBounds().contains (point.roundToInt()))
                return error ("invalid_coordinate", "Click position is outside target bounds.");

            return {};
        }

        void synthesizeComponentClick (juce::Component& target, juce::ModifierKeys buttonModifiers, int numberOfClicks, juce::Point<float> localPoint)
        {
            if (root == nullptr)
                return;

            auto source = juce::Desktop::getInstance().getMainMouseSource();
            auto now = juce::Time::getCurrentTime();

            target.mouseDown ({ source,
                                localPoint,
                                buttonModifiers,
                                1.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                &target,
                                &target,
                                now,
                                localPoint,
                                now,
                                numberOfClicks,
                                false });

            target.mouseUp ({ source,
                              localPoint,
                              juce::ModifierKeys(),
                              0.0f,
                              0.0f,
                              0.0f,
                              0.0f,
                              0.0f,
                              &target,
                              &target,
                              now + juce::RelativeTime::milliseconds (2),
                              localPoint,
                              now,
                              numberOfClicks,
                              false });

            if (numberOfClicks >= 2)
            {
                target.mouseDoubleClick ({ source,
                                           localPoint,
                                           buttonModifiers,
                                           1.0f,
                                           0.0f,
                                           0.0f,
                                           0.0f,
                                           0.0f,
                                           &target,
                                           &target,
                                           now + juce::RelativeTime::milliseconds (3),
                                           localPoint,
                                           now,
                                           numberOfClicks,
                                           false });
            }
        }

        static int getDragSteps (juce::DynamicObject& params)
        {
            return juce::jlimit (1, 100, getInt (params, "steps", 1));
        }

        void synthesizeDragOn (juce::Component& target, juce::Point<int> rootStart, juce::Point<int> rootEnd, int steps)
        {
            auto* coordinateRoot = coordinateRootFor (target);

            if (coordinateRoot == nullptr)
                return;

            if (&target == coordinateRoot)
            {
                if (auto* peer = coordinateRoot->getPeer())
                {
                    const auto toPeerPoint = [peer, coordinateRoot] (juce::Point<int> rootPoint)
                    {
                        return peer->getComponent().getLocalPoint (coordinateRoot, rootPoint).toFloat();
                    };

                    auto now = juce::Time::currentTimeMillis();
                    auto downModifiers = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);

                    peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                                            toPeerPoint (rootStart),
                                            juce::ModifierKeys(),
                                            0.0f,
                                            0.0f,
                                            now);
                    peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                                            toPeerPoint (rootStart),
                                            downModifiers,
                                            1.0f,
                                            0.0f,
                                            now + 1);

                    for (int i = 1; i <= steps; ++i)
                    {
                        juce::Point<int> point {
                            rootStart.x + (rootEnd.x - rootStart.x) * i / steps,
                            rootStart.y + (rootEnd.y - rootStart.y) * i / steps
                        };
                        peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                                                toPeerPoint (point),
                                                downModifiers,
                                                1.0f,
                                                0.0f,
                                                now + 16 * i);
                    }

                    peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                                            toPeerPoint (rootEnd),
                                            juce::ModifierKeys(),
                                            0.0f,
                                            0.0f,
                                            now + 16 * steps + 1);
                    return;
                }
            }

            auto start = target.getLocalPoint (coordinateRoot, rootStart).toFloat();
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            auto now = juce::Time::getCurrentTime();
            auto downModifiers = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);

            target.mouseDown ({ source,
                                start,
                                downModifiers,
                                1.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                &target,
                                &target,
                                now,
                                start,
                                now,
                                1,
                                false });

            juce::Point<float> end;

            for (int i = 1; i <= steps; ++i)
            {
                end = {
                    start.x + (float) (rootEnd.x - rootStart.x) * (float) i / (float) steps,
                    start.y + (float) (rootEnd.y - rootStart.y) * (float) i / (float) steps
                };

                target.mouseDrag ({ source,
                                    end,
                                    downModifiers,
                                    1.0f,
                                    0.0f,
                                    0.0f,
                                    0.0f,
                                    0.0f,
                                    &target,
                                    &target,
                                    now + juce::RelativeTime::milliseconds (16 * i),
                                    start,
                                    now,
                                    1,
                                    true });
            }

            target.mouseUp ({ source,
                              end,
                              juce::ModifierKeys(),
                              0.0f,
                              0.0f,
                              0.0f,
                              0.0f,
                              0.0f,
                              &target,
                              &target,
                              now + juce::RelativeTime::milliseconds (16 * steps + 1),
                              start,
                              now,
                              1,
                              true });
        }

        juce::Component* findComponentAt(juce::Component& component, juce::Point<int> localPoint) const {
            return component.getComponentAt(localPoint);
        }

        void synthesizeClickAt (juce::Component& coordinateRoot, juce::Point<int> rootPoint)
        {
            if (root == nullptr)
                return;

            if (auto* peer = coordinateRoot.getPeer())
            {
                auto peerPoint = peer->getComponent().getLocalPoint (&coordinateRoot, rootPoint).toFloat();
                auto now = juce::Time::currentTimeMillis();
                peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse, peerPoint, juce::ModifierKeys(), 0.0f, 0.0f, now);
                peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse, peerPoint, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, now + 1);
                peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse, peerPoint, juce::ModifierKeys(), 0.0f, 0.0f, now + 2);
            }
        }

        void sendPeerMouseEvent (juce::Component& coordinateRoot, juce::Point<int> rootPoint, juce::ModifierKeys modifiers, float pressure)
        {
            if (root == nullptr)
                return;

            if (auto* peer = coordinateRoot.getPeer())
            {
                auto peerPoint = peer->getComponent().getLocalPoint (&coordinateRoot, rootPoint).toFloat();
                peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                                        peerPoint,
                                        modifiers,
                                        pressure,
                                        0.0f,
                                        juce::Time::currentTimeMillis());
            }
        }

        static juce::Point<int> pointFromParams (juce::DynamicObject& params, const juce::Identifier& xName, const juce::Identifier& yName)
        {
            return { getInt (params, xName, 0), getInt (params, yName, 0) };
        }

        juce::Array<AutomationWindow> automationWindows() const
        {
            juce::Array<AutomationWindow> result;

            auto* rootComponent = root.getComponent();

            if (rootComponent == nullptr)
                return result;

            result.add ({ "root", rootComponent, true });

            auto* rootTopLevel = rootComponent->getTopLevelComponent();
            auto& desktop = juce::Desktop::getInstance();

            for (int i = 0; i < desktop.getNumComponents(); ++i)
            {
                auto* component = desktop.getComponent (i);

                if (component == nullptr
                    || component == rootComponent
                    || component == rootTopLevel
                    || !component->isShowing())
                {
                    continue;
                }

                result.add ({ "window-" + juce::String (result.size()), component, false });
            }

            return result;
        }

        juce::Component* automationWindowForId (const juce::String& id) const
        {
            for (auto& window : automationWindows())
                if (window.id == id)
                    return window.component.getComponent();

            return nullptr;
        }

        juce::Component* coordinateRootFor (juce::Component& component) const
        {
            auto* rootComponent = root.getComponent();

            if (rootComponent == nullptr)
                return nullptr;

            if (&component == rootComponent || rootComponent->isParentOf (&component))
                return rootComponent;

            if (auto* topLevel = component.getTopLevelComponent())
                return topLevel;

            return &component;
        }

        juce::Component* pointerCoordinateRoot (juce::DynamicObject& params) const
        {
            auto target = getString (params, "target", "root");

            if (target.isEmpty() || target == "root")
                return root.getComponent();

            return automationWindowForId (target);
        }

        juce::Rectangle<int> getRootBounds (juce::Component& component) const
        {
            auto* coordinateRoot = coordinateRootFor (component);

            if (coordinateRoot == nullptr)
                return {};

            if (&component == coordinateRoot)
                return coordinateRoot->getLocalBounds();

            if (auto* parent = component.getParentComponent())
                return coordinateRoot->getLocalArea (parent, component.getBounds());

            return component.getLocalBounds();
        }
