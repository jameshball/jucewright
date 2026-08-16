        SnapshotOptions snapshotOptions (juce::DynamicObject& params) const
        {
            SnapshotOptions snapshotOpts;
            snapshotOpts.mode = getString (params, "mode", "interesting").trim().toLowerCase();

            if ((bool) params.getProperty ("full"))
                snapshotOpts.mode = "full";

            if ((bool) params.getProperty ("interesting"))
                snapshotOpts.mode = "interesting";

            if ((bool) params.getProperty ("minimal"))
                snapshotOpts.mode = "minimal";

            snapshotOpts.includeHidden = getBool (params, "includeHidden", false);
            snapshotOpts.includeDisabled = getBool (params, "includeDisabled", true);
            snapshotOpts.includeActions = getBool (params, "includeActions", true);
            snapshotOpts.includeBounds = getBool (params, "includeBounds", true);
            snapshotOpts.maxNodes = juce::jlimit (1, 5000, getInt (params, "maxNodes", snapshotOpts.mode == "minimal" ? 200 : 400));
            snapshotOpts.maxChildrenPerContainer = juce::jlimit (1, 500, getInt (params, "maxChildrenPerContainer", snapshotOpts.mode == "minimal" ? 12 : 25));
            snapshotOpts.maxTextLength = juce::jlimit (16, 2048, getInt (params, "maxTextLength", snapshotOpts.mode == "minimal" ? 80 : 120));

            if (snapshotOpts.mode == "minimal" && params.getProperty ("includeBounds").isVoid())
                snapshotOpts.includeBounds = false;

            return snapshotOpts;
        }

        juce::var scopedSnapshotTree (juce::DynamicObject& params, juce::Component* refTarget, int maxDepth)
        {
            if (refTarget != nullptr)
                return serializeComponent (*refTarget, 0, maxDepth);

            auto target = getString (params, "target", {});

            if (target.isNotEmpty() && target != "root")
            {
                if (auto* targetWindow = automationWindowForId (target))
                    return serializeComponent (*targetWindow, 0, maxDepth);

                return error ("window_not_found", "No automation window matched target: " + target);
            }

            return serializeAutomationTree (maxDepth);
        }

        static juce::var interestingSnapshotTree (const juce::var& tree, const SnapshotOptions& options)
        {
            auto remainingNodes = options.maxNodes;
            return filterInterestingNode (tree, options, 0, remainingNodes);
        }

        static juce::var filterInterestingNode (const juce::var& node, const SnapshotOptions& options, int depth, int& remainingNodes)
        {
            auto* source = node.getDynamicObject();

            if (source == nullptr || remainingNodes <= 0)
                return {};

            const auto visible = (bool) source->getProperty ("visible");
            const auto enabled = (bool) source->getProperty ("enabled");

            if (depth > 0 && !options.includeHidden && !visible)
                return {};

            if (depth > 0 && !options.includeDisabled && !enabled)
                return {};

            juce::Array<juce::var> filteredChildren;
            int omittedChildren = 0;
            auto sourceChildren = source->getProperty ("children");

            if (sourceChildren.isArray())
            {
                for (auto& child : *sourceChildren.getArray())
                {
                    auto filteredChild = filterInterestingNode (child, options, depth + 1, remainingNodes);

                    if (!filteredChild.isVoid() && filteredChildren.size() < options.maxChildrenPerContainer)
                    {
                        filteredChildren.add (filteredChild);
                    }
                    else
                    {
                        ++omittedChildren;
                    }
                }
            }

            const auto interesting = depth == 0 || isInterestingNode (*source);

            if (!interesting && filteredChildren.isEmpty())
                return {};

            --remainingNodes;

            auto copy = compactSnapshotNode (*source, options);
            auto* copyObject = copy.getDynamicObject();

            if (copyObject != nullptr)
            {
                copyObject->setProperty ("children", filteredChildren);

                if (omittedChildren > 0)
                    copyObject->setProperty ("omittedChildren", omittedChildren);

                if (options.includeActions)
                {
                    auto actions = actionHintsForNode (*source);

                    if (actions.isArray() && !actions.getArray()->isEmpty())
                        copyObject->setProperty ("actions", actions);
                }
            }

            return copy;
        }

        static juce::var compactSnapshotNode (juce::DynamicObject& source, const SnapshotOptions& options)
        {
            auto* node = new juce::DynamicObject();

            for (auto name : { "ref",
                               "name",
                               "componentId",
                               "componentName",
                               "class",
                               "accessible",
                               "role",
                               "title",
                               "value",
                               "enabled",
                               "visible",
                               "focused",
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
                               "documentCount",
                               "layoutMode",
                               "activeDocument" })
            {
                copyCompactProperty (*node, source, name, options);
            }

            if (options.includeBounds)
                copyCompactProperty (*node, source, "bounds", options);

            auto optionsValue = source.getProperty ("options");

            if (optionsValue.isArray())
            {
                juce::Array<juce::var> compactOptions;
                auto* sourceOptions = optionsValue.getArray();
                const auto optionCount = sourceOptions != nullptr ? sourceOptions->size() : 0;
                const auto optionsToCopy = juce::jmin (optionCount, options.maxChildrenPerContainer);

                for (int i = 0; i < optionsToCopy; ++i)
                    compactOptions.add (sourceOptions->getReference (i));

                node->setProperty ("options", compactOptions);

                if (optionCount > optionsToCopy)
                    node->setProperty ("omittedOptions", optionCount - optionsToCopy);
            }

            return juce::var (node);
        }

        static void copyCompactProperty (juce::DynamicObject& target,
                                         juce::DynamicObject& source,
                                         const juce::Identifier& name,
                                         const SnapshotOptions& options)
        {
            auto value = source.getProperty (name);

            if (value.isVoid())
                return;

            if (value.isString())
            {
                auto text = value.toString();

                if (text.length() > options.maxTextLength)
                    value = text.substring (0, options.maxTextLength) + "...";
            }

            target.setProperty (name, value);
        }

        static bool isInterestingNode (juce::DynamicObject& node)
        {
            if (!isLocatorExposedNode (node) && !isSemanticContainer (node))
                return false;

            if (hasMeaningfulState (node) || isActionableNode (node) || isSemanticContainer (node))
                return true;

            const auto role = node.getProperty ("role").toString();

            if ((role == "label" || role == "staticText" || role == "image") && hasMeaningfulText (node))
                return true;

            if (hasMeaningfulIdentifier (node) && ((bool) node.getProperty ("visible") || hasMeaningfulState (node)))
                return true;

            return false;
        }

        static bool isAccessibleNode (juce::DynamicObject& node)
        {
            auto accessible = node.getProperty ("accessible");
            return accessible.isVoid() || (bool) accessible;
        }

        static bool isLocatorExposedNode (juce::DynamicObject& node)
        {
            if (isAccessibleNode (node))
                return true;

            if (hasMeaningfulIdentifier (node))
                return true;

            const auto className = node.getProperty ("class").toString();
            return className.contains ("Button")
                   || className.contains ("Slider")
                   || className.contains ("TextEditor")
                   || className.contains ("CodeEditor")
                   || className.contains ("ComboBox")
                   || className.contains ("ListBox")
                   || className.contains ("TreeView")
                   || className.contains ("Viewport")
                   || className.contains ("DocumentWindow")
                   || className.contains ("AlertWindow");
        }

        static bool hasMeaningfulIdentifier (juce::DynamicObject& node)
        {
            return node.getProperty ("componentId").toString().isNotEmpty()
                   || node.getProperty ("componentName").toString().isNotEmpty()
                   || node.getProperty ("title").toString().isNotEmpty();
        }

        static bool hasMeaningfulText (juce::DynamicObject& node)
        {
            return node.getProperty ("name").toString().trim().isNotEmpty()
                   || node.getProperty ("title").toString().trim().isNotEmpty()
                   || node.getProperty ("value").toString().trim().isNotEmpty();
        }

        static bool hasMeaningfulState (juce::DynamicObject& node)
        {
            if ((bool) node.getProperty ("focused")
                || (bool) node.getProperty ("selected")
                || (bool) node.getProperty ("expanded")
                || (bool) node.getProperty ("collapsed")
                || (bool) node.getProperty ("toggleState")
                || (bool) node.getProperty ("checked")
                || (bool) node.getProperty ("editable"))
                return true;

            if (!node.getProperty ("selectedIndex").isVoid() && (int) node.getProperty ("selectedIndex") >= 0)
                return true;

            if (!node.getProperty ("selectedId").isVoid() && (int) node.getProperty ("selectedId") != 0)
                return true;

            if (node.getProperty ("selectedText").toString().isNotEmpty()
                || node.getProperty ("currentTab").toString().isNotEmpty()
                || node.getProperty ("activeDocument").toString().isNotEmpty())
                return true;

            if (!node.getProperty ("rowCount").isVoid() && (int) node.getProperty ("rowCount") > 0)
                return true;

            if (!node.getProperty ("selectedRow").isVoid() && (int) node.getProperty ("selectedRow") >= 0)
                return true;

            if (!node.getProperty ("documentCount").isVoid() && (int) node.getProperty ("documentCount") > 0)
                return true;

            if ((!node.getProperty ("scrollX").isVoid() && (int) node.getProperty ("scrollX") != 0)
                || (!node.getProperty ("scrollY").isVoid() && (int) node.getProperty ("scrollY") != 0))
                return true;

            return false;
        }

        static bool isActionableNode (juce::DynamicObject& node)
        {
            if (!isLocatorExposedNode (node))
                return false;

            const auto role = node.getProperty ("role").toString();
            const auto className = node.getProperty ("class").toString();

            if (role == "button"
                || role == "toggleButton"
                || role == "radioButton"
                || role == "comboBox"
                || role == "slider"
                || role == "editableText"
                || role == "menuItem"
                || role == "listItem"
                || role == "treeItem"
                || role == "scrollBar"
                || role == "hyperlink")
            {
                return true;
            }

            return className.contains ("Button")
                   || className.contains ("Slider")
                   || className.contains ("TextEditor")
                   || className.contains ("CodeEditor")
                   || className.contains ("ComboBox")
                   || className.contains ("TabbedComponent")
                   || className.contains ("Viewport")
                   || className.contains ("ListBox")
                   || className.contains ("TableListBox")
                   || className.contains ("TreeView");
        }

        static bool isSemanticContainer (juce::DynamicObject& node)
        {
            const auto role = node.getProperty ("role").toString();
            const auto className = node.getProperty ("class").toString();

            return role == "window"
                   || role == "dialogWindow"
                   || role == "popupMenu"
                   || role == "table"
                   || role == "tree"
                   || role == "list"
                   || className.contains ("TabbedComponent")
                   || className.contains ("Viewport")
                   || className.contains ("ListBox")
                   || className.contains ("TableListBox")
                   || className.contains ("TreeView")
                   || className.contains ("MultiDocumentPanel")
                   || className.contains ("DocumentWindow")
                   || className.contains ("AlertWindow");
        }

        static juce::var actionHintsForNode (juce::DynamicObject& node)
        {
            juce::StringArray actions;
            const auto role = node.getProperty ("role").toString();
            const auto className = node.getProperty ("class").toString();
            const auto enabled = node.getProperty ("enabled").isVoid() || (bool) node.getProperty ("enabled");

            if (!enabled || !isLocatorExposedNode (node))
                return stringArrayToVar (actions);

            auto add = [&actions] (const juce::String& action) {
                if (!actions.contains (action))
                    actions.add (action);
            };

            if (role == "button" || className.contains ("Button"))
                add ("click");

            if ((bool) node.getProperty ("toggleable") || role == "toggleButton" || role == "radioButton")
            {
                add ("click");
                add ("set_checked");
            }

            if (role == "slider" || className.contains ("Slider"))
            {
                add ("set_value");
                add ("drag");
            }

            if (role == "editableText" || className.contains ("TextEditor") || className.contains ("CodeEditor") || (bool) node.getProperty ("editable"))
            {
                add ("fill");
                add ("clear");
                add ("press");
                add ("click");
            }

            if (role == "comboBox" || className.contains ("ComboBox") || node.getProperty ("options").isArray())
            {
                add ("select_option");
                add ("click");
            }

            if (className.contains ("TabbedComponent") || node.getProperty ("tabNames").isArray())
            {
                add ("select_tab");
                add ("click");
            }

            if (className.contains ("ListBox") || role == "list")
            {
                add ("select_option");
                add ("click");
            }

            if (role == "menuItem")
            {
                add ("click");
                add ("select_option");
            }

            if (role == "listItem")
                add ("click");

            if (className.contains ("Viewport") || role == "scrollBar")
                add ("wheel");

            if (role == "tree" || role == "treeItem" || className.contains ("TreeView"))
                add ("click");

            if (role == "window" || role == "dialogWindow" || className.contains ("DocumentWindow") || className.contains ("AlertWindow"))
                add ("snapshot");

            return stringArrayToVar (actions);
        }

        static void collectLocatorMatchNodes (juce::Array<juce::var>& matches,
                                              const juce::var& node,
                                              juce::DynamicObject& locatorObject,
                                              bool defaultVisible)
        {
            auto* object = node.getDynamicObject();

            if (object == nullptr)
                return;

            if (matchesLocator (*object, locatorObject, defaultVisible))
                matches.add (node);

            auto children = object->getProperty ("children");

            if (children.isArray())
                for (auto& child : *children.getArray())
                    collectLocatorMatchNodes (matches, child, locatorObject, defaultVisible);
        }

        static juce::var summarizeNodesForContext (const juce::Array<juce::var>& nodes)
        {
            juce::Array<juce::var> summaries;
            const auto count = juce::jmin (nodes.size(), 10);

            for (int i = 0; i < count; ++i)
                if (auto* object = nodes[i].getDynamicObject())
                    summaries.add (summarizeNode (*object));

            return juce::var (summaries);
        }

        static juce::var locatorError (const juce::String& code,
                                       const juce::String& message,
                                       juce::DynamicObject& locatorObject,
                                       const juce::Array<juce::var>& matches)
        {
            return object ({ { "__error", code },
                             { "message", message },
                             { "locator", locatorSummary (locatorObject) },
                             { "matchCount", matches.size() },
                             { "matches", summarizeNodesForContext (matches) },
                             { "suggestedNextCommand", "snapshot" } });
        }

        static juce::var locatorSummary (juce::DynamicObject& locatorObject)
        {
            auto* summary = new juce::DynamicObject();

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
                               "enabled",
                               "focused",
                               "selected" })
            {
                auto value = locatorObject.getProperty (name);

                if (!value.isVoid())
                    summary->setProperty (name, value);
            }

            return juce::var (summary);
        }

        static void collectRefs (const juce::var& node, juce::Array<juce::var>& refs)
        {
            auto* object = node.getDynamicObject();

            if (object == nullptr)
                return;

            auto ref = object->getProperty ("ref").toString();

            if (ref.isNotEmpty())
                refs.add (ref);

            auto children = object->getProperty ("children");

            if (children.isArray())
                for (auto& child : *children.getArray())
                    collectRefs (child, refs);
        }

        static juce::String suggestedSnapshotScope (juce::DynamicObject& params)
        {
            auto ref = getString (params, "ref", {});

            if (ref.isNotEmpty())
                return "ref:" + ref;

            if (params.getProperty ("locator").isObject())
                return "locator";

            auto target = getString (params, "target", {});
            return target.isNotEmpty() ? target : juce::String ("root");
        }

        juce::var serializeAutomationTree (int maxDepth)
        {
            auto windows = automationWindows();

            if (windows.size() <= 1)
            {
                auto* rootComponent = root.getComponent();
                return rootComponent != nullptr ? serializeComponent (*rootComponent, 0, maxDepth) : juce::var();
            }

            auto* node = new juce::DynamicObject();
            juce::Array<juce::var> children;

            node->setProperty ("name", "Automation Windows");
            node->setProperty ("componentId", {});
            node->setProperty ("componentName", "Automation Windows");
            node->setProperty ("class", "jucewright::AutomationWindows");
            node->setProperty ("enabled", true);
            node->setProperty ("visible", true);
            node->setProperty ("accessible", true);
            node->setProperty ("focused", false);
            node->setProperty ("bounds", rectangleToVar ({ 0, 0, 0, 0 }));
            node->setProperty ("screenBounds", rectangleToVar ({ 0, 0, 0, 0 }));

            if (maxDepth > 0)
            {
                for (auto& window : windows)
                    if (auto* component = window.component.getComponent())
                        children.add (serializeComponent (*component, 1, maxDepth));
            }

            node->setProperty ("children", children);
            return node;
        }

        juce::var serializeComponent (juce::Component& component, int depth, int maxDepth)
        {
            auto* node = new juce::DynamicObject();
            auto ref = "m" + juce::String (generation) + "-" + juce::String (refs.size() + 1);
            refs.add ({ ref, &component });

            node->setProperty ("ref", ref);
            node->setProperty ("name", componentString (&component));
            node->setProperty ("componentId", component.getComponentID());
            node->setProperty ("componentName", component.getName());
            node->setProperty ("class", type (component));
            node->setProperty ("enabled", component.isEnabled());
            const auto bounds = getRootBounds (component);
            node->setProperty ("visible", component.isShowing() && !bounds.isEmpty());
            node->setProperty ("accessible", component.isAccessible());
            node->setProperty ("focused", component.hasKeyboardFocus (false));
            node->setProperty ("bounds", rectangleToVar (bounds));
            node->setProperty ("screenBounds", rectangleToVar (component.getScreenBounds()));

            if (component.isAccessible() && component.getAccessibilityHandler() != nullptr)
            {
                auto* handler = component.getAccessibilityHandler();
                node->setProperty ("role", accessibilityRoleName (handler->getRole()));
                node->setProperty ("title", handler->getTitle());

                auto state = handler->getCurrentState();
                node->setProperty ("selectable", state.isSelectable() || state.isMultiSelectable());
                node->setProperty ("selected", state.isSelected());
                node->setProperty ("expandable", state.isExpandable());
                node->setProperty ("expanded", state.isExpanded());
                node->setProperty ("collapsed", state.isCollapsed());

                if (handler->getValueInterface() != nullptr)
                    node->setProperty ("value", handler->getValueInterface()->getCurrentValueAsString());
            }

            if (auto* button = dynamic_cast<juce::Button*> (&component))
            {
                node->setProperty ("toggleable", button->isToggleable());
                node->setProperty ("toggleState", button->getToggleState());
                node->setProperty ("checked", button->getToggleState());
            }

            if (auto* editor = dynamic_cast<juce::TextEditor*> (&component))
            {
                node->setProperty ("editable", ! editor->isReadOnly());
                node->setProperty ("readOnly", editor->isReadOnly());
                node->setProperty ("value", editor->getText());
            }

            if (auto* codeEditor = dynamic_cast<juce::CodeEditorComponent*> (&component))
            {
                node->setProperty ("editable", ! codeEditor->isReadOnly());
                node->setProperty ("readOnly", codeEditor->isReadOnly());
                node->setProperty ("value", codeEditor->getDocument().getAllContent());
            }

            if (auto* label = dynamic_cast<juce::Label*> (&component))
            {
                node->setProperty ("editable", label->isEditable());
                node->setProperty ("readOnly", ! label->isEditable());
                node->setProperty ("value", label->getText());
            }

            if (auto* slider = dynamic_cast<juce::Slider*> (&component))
            {
                node->setProperty ("value", slider->getValue());
                node->setProperty ("minimum", slider->getMinimum());
                node->setProperty ("maximum", slider->getMaximum());
                node->setProperty ("interval", slider->getInterval());
            }

            if (auto* combo = dynamic_cast<juce::ComboBox*> (&component))
            {
                node->setProperty ("value", combo->getText());
                node->setProperty ("selectedIndex", combo->getSelectedItemIndex());
                node->setProperty ("selectedId", combo->getSelectedId());
                node->setProperty ("selectedText", combo->getText());
                node->setProperty ("options", comboOptionsToVar (*combo));
            }

            if (auto* menuBar = dynamic_cast<juce::MenuBarComponent*> (&component))
            {
                if (auto* model = menuBar->getModel())
                    node->setProperty ("menus", stringArrayToVar (model->getMenuBarNames()));

                node->setProperty ("options", menuBarItemsToVar (*menuBar));
            }

            if (auto* tabs = dynamic_cast<juce::TabbedComponent*> (&component))
            {
                auto tabNames = tabs->getTabNames();
                const auto currentTabIndex = tabs->getCurrentTabIndex();

                node->setProperty ("tabNames", stringArrayToVar (tabNames));
                node->setProperty ("currentTabIndex", currentTabIndex);

                if (juce::isPositiveAndBelow (currentTabIndex, tabNames.size()))
                    node->setProperty ("currentTab", tabNames[currentTabIndex]);
            }

            if (auto* viewport = dynamic_cast<juce::Viewport*> (&component))
            {
                node->setProperty ("scrollX", viewport->getViewPositionX());
                node->setProperty ("scrollY", viewport->getViewPositionY());
                node->setProperty ("viewWidth", viewport->getViewWidth());
                node->setProperty ("viewHeight", viewport->getViewHeight());

                if (auto* viewed = viewport->getViewedComponent())
                {
                    node->setProperty ("contentWidth", viewed->getWidth());
                    node->setProperty ("contentHeight", viewed->getHeight());
                }
            }

            if (auto* listBox = dynamic_cast<juce::ListBox*> (&component))
            {
                node->setProperty ("rowCount", listBox->getListBoxModel() != nullptr ? listBox->getListBoxModel()->getNumRows() : 0);
                node->setProperty ("selectedRow", listBox->getSelectedRow());
                node->setProperty ("selectedRows", selectedRowsToVar (*listBox));
                node->setProperty ("selectedText", listRowName (*listBox, listBox->getSelectedRow()));
                node->setProperty ("options", listRowsToVar (*listBox));
            }

            if (auto* multiPanel = dynamic_cast<juce::MultiDocumentPanel*> (&component))
            {
                node->setProperty ("documentCount", multiPanel->getNumDocuments());
                node->setProperty ("layoutMode", multiPanel->getLayoutMode() == juce::MultiDocumentPanel::FloatingWindows ? "floating"
                                                                                                                            : "tabs");

                if (auto* activeDocument = multiPanel->getActiveDocument())
                    node->setProperty ("activeDocument", activeDocument->getName());
            }

            juce::Array<juce::var> children;

            if (depth < maxDepth)
                addSerializedChildren (children, component, depth + 1, maxDepth);

            node->setProperty ("children", children);
            return node;
        }

        static juce::var stringArrayToVar (const juce::StringArray& values)
        {
            juce::Array<juce::var> result;

            for (const auto& value : values)
                result.add (value);

            return result;
        }

        static juce::StringArray stringArrayFromVar (const juce::var& value)
        {
            juce::StringArray result;

            if (value.isArray())
            {
                for (const auto& item : *value.getArray())
                    result.add (item.toString());
            }
            else if (!value.isVoid())
            {
                result.add (value.toString());
            }

            return result;
        }

        static juce::var comboOptionsToVar (juce::ComboBox& combo)
        {
            juce::Array<juce::var> result;

            for (int i = 0; i < combo.getNumItems(); ++i)
                result.add (object ({ { "index", i },
                                      { "id", combo.getItemId (i) },
                                      { "text", combo.getItemText (i) } }));

            return result;
        }

        static juce::var menuBarItemsToVar (juce::MenuBarComponent& menuBar)
        {
            juce::Array<juce::var> result;
            auto* model = menuBar.getModel();

            if (model == nullptr)
                return result;

            auto menuNames = model->getMenuBarNames();
            int flatIndex = 0;

            for (int menuIndex = 0; menuIndex < menuNames.size(); ++menuIndex)
            {
                const auto menuName = menuNames[menuIndex];
                auto menu = model->getMenuForIndex (menuIndex, menuName);
                juce::PopupMenu::MenuItemIterator iterator (menu, true);

                while (iterator.next())
                {
                    auto& item = iterator.getItem();

                    if (item.isSeparator || item.isSectionHeader)
                        continue;

                    result.add (object ({ { "index", flatIndex },
                                          { "id", item.itemID },
                                          { "menu", menuName },
                                          { "text", item.text },
                                          { "enabled", item.isEnabled },
                                          { "checked", item.isTicked },
                                          { "separator", item.isSeparator },
                                          { "sectionHeader", item.isSectionHeader },
                                          { "hasSubMenu", item.subMenu != nullptr } }));

                    ++flatIndex;
                }
            }

            return result;
        }

        static juce::String listRowName (juce::ListBox& listBox, int row)
        {
            auto* model = listBox.getListBoxModel();

            if (model == nullptr || ! juce::isPositiveAndBelow (row, model->getNumRows()))
                return {};

            return model->getNameForRow (row);
        }

        static juce::var listRowsToVar (juce::ListBox& listBox)
        {
            juce::Array<juce::var> result;
            auto* model = listBox.getListBoxModel();

            if (model == nullptr)
                return result;

            const auto rowsToExpose = juce::jmin (model->getNumRows(), 200);

            for (int i = 0; i < rowsToExpose; ++i)
                result.add (object ({ { "index", i }, { "text", model->getNameForRow (i) } }));

            return result;
        }

        static juce::var selectedRowsToVar (juce::ListBox& listBox)
        {
            juce::Array<juce::var> result;
            auto selectedRows = listBox.getSelectedRows();

            for (int i = 0; i < selectedRows.size(); ++i)
                result.add (selectedRows[i]);

            return result;
        }

        void addSerializedChildren (juce::Array<juce::var>& children, juce::Component& component, int depth, int maxDepth)
        {
            juce::Component* serializedTabbedDocument = nullptr;

            if (auto* multiPanel = dynamic_cast<juce::MultiDocumentPanel*> (&component))
                if ((serializedTabbedDocument = multiPanel->getCurrentTabbedComponent()) != nullptr)
                    children.add (serializeComponent (*serializedTabbedDocument, depth, maxDepth));

            if (auto* tabs = dynamic_cast<juce::TabbedComponent*> (&component))
            {
                for (int i = 0; i < tabs->getNumTabs(); ++i)
                    if (auto* child = tabs->getTabContentComponent (i))
                        children.add (serializeComponent (*child, depth, maxDepth));

                return;
            }

            for (int i = 0; i < component.getNumChildComponents(); ++i)
            {
                auto* child = component.getChildComponent (i);

                if (child == nullptr || child == serializedTabbedDocument)
                    continue;

                children.add (serializeComponent (*child, depth, maxDepth));
            }
        }
