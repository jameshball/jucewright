    juce::var stringSchema()
    {
        return object ({ { "type", "string" } });
    }

    juce::var numberSchema()
    {
        return object ({ { "type", "number" } });
    }

    juce::var timeoutMsSchema()
    {
        return object ({ { "type", "number" }, { "default", 5000 } });
    }

    juce::var booleanSchema()
    {
        return object ({ { "type", "boolean" } });
    }

    juce::var stringArraySchema()
    {
        return object ({ { "type", "array" }, { "items", stringSchema() } });
    }

    juce::var locatorSchema()
    {
        return object ({ { "type", "object" },
                         { "properties", object ({ { "role", stringSchema() },
                                                   { "name", stringSchema() },
                                                   { "text", stringSchema() },
                                                   { "componentId", stringSchema() },
                                                   { "componentName", stringSchema() },
                                                   { "testId", stringSchema() },
                                                   { "class", stringSchema() },
                                                   { "value", stringSchema() },
                                                   { "hasText", stringSchema() },
                                                   { "exact", booleanSchema() },
                                                   { "visible", booleanSchema() },
                                                   { "accessible", booleanSchema() },
                                                   { "enabled", booleanSchema() },
                                                   { "focused", booleanSchema() },
                                                   { "selected", booleanSchema() },
                                                   { "nth", numberSchema() } }) } });
    }

    juce::var toolSchema (std::initializer_list<std::pair<juce::String, juce::var>> properties,
                          std::initializer_list<juce::var> required = {})
    {
        auto schema = object ({ { "type", "object" }, { "properties", object (properties) } });

        if (required.size() > 0)
            schema.getDynamicObject()->setProperty ("required", array (required));

        return schema;
    }

    juce::var timedToolSchema (std::initializer_list<std::pair<juce::String, juce::var>> properties,
                               std::initializer_list<juce::var> required = {})
    {
        auto schema = toolSchema (properties, required);

        if (auto* schemaObject = schema.getDynamicObject())
            if (auto* propertiesObject = schemaObject->getProperty ("properties").getDynamicObject())
                propertiesObject->setProperty ("timeoutMs", timeoutMsSchema());

        return schema;
    }

    juce::var targetActionToolSchema (std::initializer_list<std::pair<juce::String, juce::var>> properties,
                                      std::initializer_list<juce::var> required = {})
    {
        auto schema = timedToolSchema (properties, required);

        if (auto* schemaObject = schema.getDynamicObject())
        {
            if (auto* propertiesObject = schemaObject->getProperty ("properties").getDynamicObject())
            {
                propertiesObject->setProperty ("force", booleanSchema());
                propertiesObject->setProperty ("trial", booleanSchema());
            }
        }

        return schema;
    }

    juce::var tool (const juce::String& name, const juce::String& description, const juce::var& inputSchema)
    {
        return object ({ { "name", name }, { "description", description }, { "inputSchema", inputSchema } });
    }

    juce::var mcpTools()
    {
        return array ({
            tool ("juce_list_sessions",
                  "List running jucewright automation sessions.",
                  toolSchema ({})),
            tool ("juce_windows",
                  "List automation-owned JUCE windows for a session.",
                  toolSchema ({ { "session", stringSchema() } })),
            tool ("juce_trace_start",
                  "Start recording automation trace events.",
                  toolSchema ({ { "session", stringSchema() }, { "file", stringSchema() } })),
            tool ("juce_trace_stop",
                  "Stop recording automation trace events and write the trace artifact.",
                  toolSchema ({ { "session", stringSchema() } })),
            tool ("juce_capabilities",
                  "Return protocol, feature, and security capabilities for a running automation session.",
                  toolSchema ({ { "session", stringSchema() } })),
            tool ("juce_locator",
                  "Find visible JUCE components by Playwright-style locator fields. Pass visible=false to inspect hidden components.",
                  timedToolSchema ({ { "session", stringSchema() }, { "locator", locatorSchema() } }, { "locator" })),
            tool ("juce_count",
                  "Count visible JUCE components matching Playwright-style locator fields without returning the component tree.",
                  timedToolSchema ({ { "session", stringSchema() }, { "locator", locatorSchema() } }, { "locator" })),
            tool ("juce_describe",
                  "Describe one component by ref or strict locator, including compact state, ancestors, children, and action hints.",
                  toolSchema ({ { "session", stringSchema() },
                                { "ref", stringSchema() },
                                { "locator", locatorSchema() },
                                { "mode", object ({ { "type", "string" },
                                                     { "enum", array ({ "interesting", "full", "minimal" }) },
                                                     { "default", "interesting" } }) },
                                { "depth", object ({ { "type", "number" }, { "default", 2 } }) },
                                { "timeoutMs", timeoutMsSchema() },
                                { "includeHidden", booleanSchema() },
                                { "includeActions", booleanSchema() },
                                { "includeBounds", booleanSchema() } })),
            tool ("juce_snapshot",
                  "Return a token-efficient Playwright-style snapshot of a JUCE component tree. Defaults to mode=interesting and format=json.",
                  toolSchema ({ { "session", stringSchema() },
                                { "mode", object ({ { "type", "string" },
                                                     { "enum", array ({ "interesting", "full", "minimal" }) },
                                                     { "default", "interesting" } }) },
                                { "format", object ({ { "type", "string" },
                                                       { "enum", array ({ "text", "json" }) },
                                                       { "default", "json" } }) },
                                { "depth", object ({ { "type", "number" }, { "default", 8 } }) },
                                { "target", stringSchema() },
                                { "ref", stringSchema() },
                                { "locator", locatorSchema() },
                                { "since", stringSchema() },
                                { "includeHidden", booleanSchema() },
                                { "includeDisabled", booleanSchema() },
                                { "includeActions", booleanSchema() },
                                { "includeBounds", booleanSchema() },
                                { "maxNodes", numberSchema() },
                                { "maxChildrenPerContainer", numberSchema() },
                                { "maxTextLength", numberSchema() },
                                { "timeoutMs", timeoutMsSchema() } })),
            tool ("juce_screenshot",
                  "Capture a PNG screenshot of the root or a component ref.",
                  timedToolSchema ({ { "session", stringSchema() },
                                     { "target", object ({ { "type", "string" }, { "default", "root" } }) },
                                     { "ref", stringSchema() },
                                     { "locator", locatorSchema() },
                                     { "source", object ({ { "type", "string" },
                                                           { "enum", array ({ "auto", "component", "native" }) },
                                                           { "default", "auto" } }) },
                                     { "file", stringSchema() },
                                     { "clipX", numberSchema() },
                                     { "clipY", numberSchema() },
                                     { "clipW", numberSchema() },
                                     { "clipH", numberSchema() },
                                     { "scale", numberSchema() },
                                     { "includeBase64", booleanSchema() } })),
            tool ("juce_click",
                  "Click a component ref and return a fresh snapshot.",
                  targetActionToolSchema ({ { "session", stringSchema() },
                                            { "ref", stringSchema() },
                                            { "locator", locatorSchema() },
                                            { "button", stringSchema() },
                                            { "clickCount", numberSchema() },
                                            { "position", object ({ { "type", "object" },
                                                                    { "properties", object ({ { "x", numberSchema() }, { "y", numberSchema() } }) } }) } })),
            tool ("juce_dblclick",
                  "Double-click a component ref or locator and return a fresh snapshot.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() } })),
            tool ("juce_right_click",
                  "Right-click a component ref or locator and return a fresh snapshot.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "menuItem", stringSchema() } })),
            tool ("juce_click_xy",
                  "Click window-local coordinates and return a fresh snapshot.",
                  timedToolSchema ({ { "session", stringSchema() }, { "target", stringSchema() }, { "x", numberSchema() }, { "y", numberSchema() } }, { "x", "y" })),
            tool ("juce_hover",
                  "Move the mouse to window-local coordinates.",
                  timedToolSchema ({ { "session", stringSchema() }, { "target", stringSchema() }, { "x", numberSchema() }, { "y", numberSchema() } }, { "x", "y" })),
            tool ("juce_mouse_move",
                  "Move the mouse to window-local coordinates.",
                  timedToolSchema ({ { "session", stringSchema() }, { "target", stringSchema() }, { "x", numberSchema() }, { "y", numberSchema() } }, { "x", "y" })),
            tool ("juce_mouse_down",
                  "Send a mouse-down event at window-local coordinates.",
                  timedToolSchema ({ { "session", stringSchema() }, { "target", stringSchema() }, { "x", numberSchema() }, { "y", numberSchema() } }, { "x", "y" })),
            tool ("juce_mouse_up",
                  "Send a mouse-up event at window-local coordinates.",
                  timedToolSchema ({ { "session", stringSchema() }, { "target", stringSchema() }, { "x", numberSchema() }, { "y", numberSchema() } }, { "x", "y" })),
            tool ("juce_wheel",
                  "Send a mouse wheel event at window-local coordinates.",
                  timedToolSchema ({ { "session", stringSchema() }, { "target", stringSchema() }, { "x", numberSchema() }, { "y", numberSchema() }, { "deltaX", numberSchema() }, { "deltaY", numberSchema() } }, { "x", "y" })),
            tool ("juce_drag_xy",
                  "Drag from one window-local point to another.",
                  timedToolSchema ({ { "session", stringSchema() }, { "target", stringSchema() }, { "x", numberSchema() }, { "y", numberSchema() }, { "toX", numberSchema() }, { "toY", numberSchema() }, { "steps", numberSchema() } }, { "x", "y", "toX", "toY" })),
            tool ("juce_type",
                  "Type text into a component ref and return a fresh snapshot.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "text", stringSchema() } }, { "text" })),
            tool ("juce_fill",
                  "Replace text in a TextEditor or Label.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "text", stringSchema() } }, { "text" })),
            tool ("juce_clear",
                  "Clear text from a TextEditor or Label.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() } })),
            tool ("juce_press",
                  "Press a key and return a fresh snapshot.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "key", stringSchema() } }, { "key" })),
            tool ("juce_key_down",
                  "Send a key-down event, supporting chords such as Control+K.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "key", stringSchema() } }, { "key" })),
            tool ("juce_key_up",
                  "Send a key-up event, supporting chords such as Control+K.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "key", stringSchema() } }, { "key" })),
            tool ("juce_check",
                  "Set a toggleable button checked.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() } })),
            tool ("juce_uncheck",
                  "Set a toggleable button unchecked.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() } })),
            tool ("juce_set_checked",
                  "Set a toggleable button checked or unchecked.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "checked", booleanSchema() } }, { "checked" })),
            tool ("juce_set_value",
                  "Set a semantic value on a Slider or TextEditor.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "value", numberSchema() } }, { "value" })),
            tool ("juce_select_option",
                  "Select a ComboBox option or ListBox row by text, index, or id.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "text", stringSchema() }, { "index", numberSchema() }, { "id", numberSchema() } })),
            tool ("juce_select_tab",
                  "Select a TabbedComponent tab by name or index.",
                  targetActionToolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "name", stringSchema() }, { "index", numberSchema() } })),
            tool ("juce_drag",
                  "Drag a component by a delta and return a fresh snapshot.",
                  targetActionToolSchema ({ { "session", stringSchema() },
                                            { "ref", stringSchema() },
                                            { "locator", locatorSchema() },
                                            { "dx", numberSchema() },
                                            { "dy", numberSchema() },
                                            { "steps", numberSchema() },
                                            { "position", object ({ { "type", "object" },
                                                                    { "properties", object ({ { "x", numberSchema() }, { "y", numberSchema() } }) } }) } })),
            tool ("juce_drag_to",
                  "Drag a source component to a target component center.",
                  targetActionToolSchema ({ { "session", stringSchema() },
                                            { "ref", stringSchema() },
                                            { "locator", locatorSchema() },
                                            { "targetRef", stringSchema() },
                                            { "targetLocator", locatorSchema() },
                                            { "steps", numberSchema() } })),
            tool ("juce_drop",
                  "Invoke a JUCE DragAndDropTarget with a drag description.",
                  targetActionToolSchema ({ { "session", stringSchema() },
                                            { "ref", stringSchema() },
                                            { "locator", locatorSchema() },
                                            { "description", stringSchema() },
                                            { "sourceRef", stringSchema() },
                                            { "sourceLocator", locatorSchema() },
                                            { "position", object ({ { "type", "object" },
                                                                    { "properties", object ({ { "x", numberSchema() }, { "y", numberSchema() } }) } }) } },
                                          { "description" })),
            tool ("juce_drop_files",
                  "Invoke a JUCE FileDragAndDropTarget with one or more file paths.",
                  targetActionToolSchema ({ { "session", stringSchema() },
                                            { "ref", stringSchema() },
                                            { "locator", locatorSchema() },
                                            { "file", stringSchema() },
                                            { "files", stringArraySchema() },
                                            { "position", object ({ { "type", "object" },
                                                                    { "properties", object ({ { "x", numberSchema() }, { "y", numberSchema() } }) } }) } })),
            tool ("juce_resize_window",
                  "Resize an automation window and return a fresh snapshot.",
                  toolSchema ({ { "session", stringSchema() },
                                { "target", object ({ { "type", "string" }, { "default", "root" } }) },
                                { "w", numberSchema() },
                                { "h", numberSchema() } },
                              { "w", "h" })),
            tool ("juce_wait",
                  "Wait briefly and return a fresh snapshot.",
                  toolSchema ({ { "session", stringSchema() }, { "ms", object ({ { "type", "number" }, { "default", 250 } }) } })),
            tool ("juce_wait_for_ref",
                  "Wait for a previously returned component ref to remain attached.",
                  toolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "timeoutMs", timeoutMsSchema() } }, { "ref" })),
            tool ("juce_wait_for_locator",
                  "Wait for a visible locator match unless the locator requests hidden components.",
                  toolSchema ({ { "session", stringSchema() }, { "locator", locatorSchema() }, { "timeoutMs", timeoutMsSchema() } }, { "locator" })),
            tool ("juce_wait_for_text",
                  "Wait for visible text in the component tree.",
                  toolSchema ({ { "session", stringSchema() }, { "text", stringSchema() }, { "timeoutMs", timeoutMsSchema() }, { "exact", booleanSchema() }, { "visible", booleanSchema() } }, { "text" })),
            tool ("juce_wait_for_value",
                  "Wait for a semantic component value to match.",
                  toolSchema ({ { "session", stringSchema() }, { "ref", stringSchema() }, { "locator", locatorSchema() }, { "value", stringSchema() }, { "timeoutMs", timeoutMsSchema() }, { "exact", booleanSchema() } }, { "value" })),
            tool ("juce_wait_for_snapshot_change",
                  "Wait for the snapshot state hash to differ from a previous state hash.",
                  toolSchema ({ { "session", stringSchema() }, { "stateHash", stringSchema() }, { "timeoutMs", timeoutMsSchema() } }, { "stateHash" }))
        });
    }

    juce::String methodForTool (const juce::String& name)
    {
        if (name == "juce_snapshot") return "snapshot";
        if (name == "juce_capabilities") return "capabilities";
        if (name == "juce_locator") return "locator";
        if (name == "juce_count") return "count";
        if (name == "juce_describe") return "describe";
        if (name == "juce_screenshot") return "screenshot";
        if (name == "juce_click") return "click";
        if (name == "juce_dblclick") return "dblclick";
        if (name == "juce_right_click") return "right_click";
        if (name == "juce_click_xy") return "click_xy";
        if (name == "juce_hover") return "hover";
        if (name == "juce_mouse_move") return "mouse_move";
        if (name == "juce_mouse_down") return "mouse_down";
        if (name == "juce_mouse_up") return "mouse_up";
        if (name == "juce_wheel") return "wheel";
        if (name == "juce_drag_xy") return "drag_xy";
        if (name == "juce_type") return "type";
        if (name == "juce_fill") return "fill";
        if (name == "juce_clear") return "clear";
        if (name == "juce_press") return "press";
        if (name == "juce_key_down") return "key_down";
        if (name == "juce_key_up") return "key_up";
        if (name == "juce_check") return "check";
        if (name == "juce_uncheck") return "uncheck";
        if (name == "juce_set_checked") return "set_checked";
        if (name == "juce_set_value") return "set_value";
        if (name == "juce_select_option") return "select_option";
        if (name == "juce_select_tab") return "select_tab";
        if (name == "juce_drag") return "drag";
        if (name == "juce_drag_to") return "drag_to";
        if (name == "juce_drop") return "drop";
        if (name == "juce_drop_files") return "drop_files";
        if (name == "juce_resize_window") return "resize_window";
        if (name == "juce_wait") return "wait";
        if (name == "juce_wait_for_ref") return "wait_for_ref";
        if (name == "juce_wait_for_locator") return "wait_for_locator";
        if (name == "juce_wait_for_text") return "wait_for_text";
        if (name == "juce_wait_for_value") return "wait_for_value";
        if (name == "juce_wait_for_snapshot_change") return "wait_for_snapshot_change";
        if (name == "juce_windows") return "windows";
        if (name == "juce_trace_start") return "trace_start";
        if (name == "juce_trace_stop") return "trace_stop";

        return {};
    }

    juce::var mcpTextContent (const juce::String& text)
    {
        return object ({ { "content", array ({ object ({ { "type", "text" }, { "text", text } }) }) } });
    }

    juce::var mcpEndpointErrorContent (juce::DynamicObject& responseObject)
    {
        return object ({ { "isError", true },
                         { "content", array ({ object ({ { "type", "text" },
                                                          { "text", juce::JSON::toString (responseObject.getProperty ("error"), true) } }) }) } });
    }

    juce::var callMcpTool (const juce::String& name, juce::var arguments)
    {
        if (!arguments.isObject())
            arguments = emptyObject();

        auto* args = arguments.getDynamicObject();

        if (name == "juce_list_sessions")
        {
            juce::Array<juce::var> publicSessions;

            for (const auto& session : loadSessions())
            {
                if (auto* sessionObject = session.getDynamicObject())
                {
                    publicSessions.add (object ({ { "pid", sessionObject->getProperty ("pid") },
                                                  { "session", sessionObject->getProperty ("session") },
                                                  { "root", sessionObject->getProperty ("root") },
                                                  { "host", sessionObject->getProperty ("host") },
                                                  { "port", sessionObject->getProperty ("port") },
                                                  { "file", sessionObject->getProperty ("file") },
                                                  { "modifiedAtMs", sessionObject->getProperty ("modifiedAtMs") } }));
                }
            }

            return mcpTextContent (juce::JSON::toString (juce::var (publicSessions), true));
        }

        const auto sessionName = args->getProperty ("session").toString();
        auto session = findSession (sessionName);
        auto* sessionObject = session.getDynamicObject();

        if (sessionObject == nullptr)
            throw std::runtime_error (("No jucewright automation session found"
                                       + (sessionName.isNotEmpty() ? " for '" + sessionName + "'" : juce::String()))
                                          .toStdString());

        const auto method = methodForTool (name);

        if (method.isEmpty())
            throw std::runtime_error (("Unknown Jucewright MCP tool: " + name).toStdString());

        if (name == "juce_snapshot")
        {
            if (args->getProperty ("format").isVoid())
                args->setProperty ("format", "json");

            if (args->getProperty ("mode").isVoid())
                args->setProperty ("mode", "interesting");
        }

        if (name == "juce_screenshot" && args->getProperty ("includeBase64").isVoid())
            args->setProperty ("includeBase64", true);

        auto response = requestEnvelope (*sessionObject, method, arguments);
        auto* responseObject = response.getDynamicObject();

        if (responseObject == nullptr)
            throw std::runtime_error ("Invalid response from automation endpoint");

        if (! (bool) responseObject->getProperty ("ok"))
            return mcpEndpointErrorContent (*responseObject);

        auto result = responseObject->getProperty ("result");

        if (name == "juce_screenshot")
        {
            juce::Array<juce::var> content;
            auto* resultObject = result.getDynamicObject();

            if (resultObject != nullptr)
            {
                auto file = resultObject->getProperty ("file").toString();
                auto base64 = resultObject->getProperty ("base64").toString();
                auto mimeType = resultObject->getProperty ("mimeType").toString();

                if (file.isNotEmpty())
                    content.add (object ({ { "type", "text" }, { "text", file } }));

                if (base64.isNotEmpty())
                    content.add (object ({ { "type", "image" },
                                           { "data", base64 },
                                           { "mimeType", mimeType.isNotEmpty() ? mimeType : juce::String ("image/png") } }));
            }

            return object ({ { "content", juce::var (content) } });
        }

        if (name == "juce_snapshot" && args->getProperty ("format").toString() == "json")
            return mcpTextContent (juce::JSON::toString (result, true));

        if (name == "juce_locator" || name == "juce_count" || name == "juce_describe")
            return mcpTextContent (juce::JSON::toString (result, true));

        if (auto* resultObject = result.getDynamicObject())
        {
            auto text = resultObject->getProperty ("text").toString();

            if (text.isNotEmpty())
                return mcpTextContent (text);
        }

        return mcpTextContent (juce::JSON::toString (result, true));
    }

    juce::var jsonRpcResult (const juce::var& id, const juce::var& result)
    {
        return object ({ { "jsonrpc", "2.0" }, { "id", id }, { "result", result } });
    }

    juce::var jsonRpcError (const juce::var& id, int code, const juce::String& message)
    {
        return object ({ { "jsonrpc", "2.0" },
                         { "id", id },
                         { "error", object ({ { "code", code }, { "message", message } }) } });
    }

    juce::var handleMcpLine (const juce::String& line)
    {
        auto parsed = juce::JSON::parse (line);
        auto* requestObject = parsed.getDynamicObject();

        if (requestObject == nullptr)
            return jsonRpcError ({}, -32700, "Parse error");

        auto id = requestObject->getProperty ("id");
        auto method = requestObject->getProperty ("method").toString();

        try
        {
            if (method == "initialize")
            {
                auto params = requestObject->getProperty ("params");
                auto* paramsObject = params.getDynamicObject();
                auto protocolVersion = paramsObject != nullptr ? paramsObject->getProperty ("protocolVersion").toString() : juce::String();

                if (protocolVersion.isEmpty())
                    protocolVersion = "2025-11-25";

                return jsonRpcResult (id,
                                      object ({ { "protocolVersion", protocolVersion },
                                                { "capabilities", object ({ { "tools", emptyObject() } }) },
                                                { "serverInfo", object ({ { "name", "jucewright-mcp" }, { "version", "0.1.0" } }) } }));
            }

            if (method == "tools/list")
                return jsonRpcResult (id, object ({ { "tools", mcpTools() } }));

            if (method == "tools/call")
            {
                auto params = requestObject->getProperty ("params");
                auto* paramsObject = params.getDynamicObject();

                if (paramsObject == nullptr)
                    return jsonRpcError (id, -32602, "tools/call params must be an object");

                return jsonRpcResult (id,
                                      callMcpTool (paramsObject->getProperty ("name").toString(),
                                                   paramsObject->getProperty ("arguments")));
            }

            if (id.isVoid())
                return {};

            return jsonRpcError (id, -32601, "Method not found: " + method);
        }
        catch (const std::exception& e)
        {
            return jsonRpcError (id, -32000, e.what());
        }
    }

    void runMcpServer()
    {
        std::string line;

        while (std::getline (std::cin, line))
        {
            auto response = handleMcpLine (juce::String::fromUTF8 (line.data(), (int) line.size()));

            if (!response.isVoid())
                std::cout << juce::JSON::toString (response, true).toStdString() << "\n";
        }
    }
