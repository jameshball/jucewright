    public:
        AutomationController (juce::Component& rootComponent, AutomationOptions automationOptions)
            : juce::Thread ("Jucewright Automation"),
              root (&rootComponent),
              options (std::move (automationOptions))
        {
            if (options.sessionName.isEmpty())
                options.sessionName = defaultSessionName();

            if (options.authToken.isEmpty())
                options.authToken = juce::Uuid().toString();

            listener = std::make_unique<juce::StreamingSocket>();

            if (!listener->createListener (options.port, "127.0.0.1"))
                return;

            boundPort = listener->getBoundPort();

            if (options.advertise)
                writeAdvertisement();

            startThread();
        }

        ~AutomationController() override
        {
            signalThreadShouldExit();

            if (listener)
                listener->close();

            {
                const juce::ScopedLock lock (activeClientLock);

                if (activeClient != nullptr)
                    activeClient->close();
            }

            waitForThreadToStop();
            removeAdvertisement();
        }

        void updateRoot (juce::Component& newRoot)
        {
            root = &newRoot;

            if (options.advertise)
                writeAdvertisement();
        }

        void clearRoot()
        {
            root = nullptr;
        }

        [[nodiscard]] bool isRunning() const noexcept
        {
            return listener != nullptr && boundPort > 0;
        }

        [[nodiscard]] int getPort() const noexcept
        {
            return boundPort;
        }

        [[nodiscard]] juce::String getAuthToken() const
        {
            return options.authToken;
        }

    private:
        struct ComponentRef
        {
            juce::String ref;
            juce::Component::SafePointer<juce::Component> component;
        };

        struct TargetResolution
        {
            juce::Component* component = nullptr;
            juce::var error;
        };

        struct AutomationWindow
        {
            juce::String id;
            juce::Component::SafePointer<juce::Component> component;
            bool attachedRoot = false;
        };

        struct SnapshotOptions
        {
            juce::String mode = "interesting";
            bool includeHidden = false;
            bool includeDisabled = true;
            bool includeActions = true;
            bool includeBounds = true;
            int maxNodes = 400;
            int maxChildrenPerContainer = 25;
            int maxTextLength = 120;
        };

        juce::Component::SafePointer<juce::Component> root;
        AutomationOptions options;
        std::unique_ptr<juce::StreamingSocket> listener;
        juce::CriticalSection activeClientLock;
        juce::StreamingSocket* activeClient = nullptr;
        int boundPort = -1;
        juce::File advertisementFile;
        juce::File traceFile;
        juce::Array<juce::var> traceEvents;
        juce::Array<ComponentRef> refs;
        int generation = 0;
        bool traceEnabled = false;
        bool includeActionSnapshot = true;
        static constexpr int protocolVersion = 1;

        void run() override
        {
            while (!threadShouldExit() && listener != nullptr)
            {
                std::unique_ptr<juce::StreamingSocket> client (listener->waitForNextConnection());

                if (client == nullptr)
                    continue;

                handleClient (*client);
                client->close();
            }
        }

        void handleClient (juce::StreamingSocket& client)
        {
            {
                const juce::ScopedLock lock (activeClientLock);
                activeClient = &client;
            }

            auto requestLine = readLine (client);
            auto response = handleRequest (requestLine);
            writeLine (client, response);

            {
                const juce::ScopedLock lock (activeClientLock);

                if (activeClient == &client)
                    activeClient = nullptr;
            }
        }

        juce::String readLine (juce::StreamingSocket& client)
        {
            std::string bytes;
            char buffer[1024] {};

            while (!threadShouldExit())
            {
                const auto ready = client.waitUntilReady (true, 5000);

                if (ready <= 0)
                    break;

                const auto bytesRead = client.read (buffer, (int) sizeof (buffer), false);

                if (bytesRead <= 0)
                    break;

                bytes.append (buffer, (size_t) bytesRead);

                if (bytes.find ('\n') != std::string::npos)
                    break;
            }

            if (auto newline = bytes.find ('\n'); newline != std::string::npos)
                bytes.resize (newline);

            return juce::String::fromUTF8 (bytes.data(), (int) bytes.size()).trim();
        }

        static void writeLine (juce::StreamingSocket& client, const juce::String& line)
        {
            const auto payload = line + "\n";
            auto* data = payload.toRawUTF8();
            auto bytesRemaining = (int) payload.getNumBytesAsUTF8();
            auto deadline = juce::Time::currentTimeMillis() + 30000;
            auto noProgressCount = 0;

            while (bytesRemaining > 0 && juce::Time::currentTimeMillis() < deadline)
            {
                const auto remaining = (int) (deadline - juce::Time::currentTimeMillis());
                const auto ready = client.waitUntilReady (false, juce::jlimit (1, 500, remaining));

                if (ready < 0)
                    break;

                if (ready == 0)
                    continue;

                const auto bytesToWrite = juce::jmin (bytesRemaining, 16384);
                const auto bytesWritten = client.write (data, bytesToWrite);

                if (bytesWritten <= 0)
                {
                    if (++noProgressCount > 8)
                        break;

                    juce::Thread::sleep (1);
                    continue;
                }

                noProgressCount = 0;
                data += bytesWritten;
                bytesRemaining -= bytesWritten;
            }
        }

        juce::String handleRequest (const juce::String& requestLine)
        {
            auto request = juce::JSON::parse (requestLine);
            auto* requestObject = request.getDynamicObject();

            if (requestObject == nullptr)
                return responseError ({}, "invalid_json", "Request must be a JSON object.");

            const auto id = getString (*requestObject, "id", {});

            if (getString (*requestObject, "token", {}) != options.authToken)
                return responseError (id, "unauthorized", "Invalid automation token.");

            const auto method = getString (*requestObject, "method", {});
            auto params = requestObject->getProperty ("params");
            auto* paramsObject = params.getDynamicObject();

            juce::DynamicObject emptyParams;

            if (paramsObject == nullptr)
                paramsObject = &emptyParams;

            const auto startMs = juce::Time::getMillisecondCounterHiRes();
            auto result = dispatchRequest (method, *paramsObject);
            const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - startMs;

            recordTraceEvent (method, *paramsObject, result, elapsedMs);

            if (auto* resultObject = result.getDynamicObject())
            {
                if (resultObject->getProperty ("__error").isString())
                    return responseError (id, *resultObject);
            }

            return responseOk (id, result);
        }

        juce::var dispatchRequest (const juce::String& method, juce::DynamicObject& params)
        {
            if (method == "wait")
                sleepUntilReadyOrStopped (juce::jlimit (0, 30000, getInt (params, "ms", 250)));

            if (isWaitForMethod (method))
                return waitForCondition (method, params);

            if (!isAutoWaitMethod (method))
                return callOnMessageThread ([this, method, &params]() {
                    return dispatch (method, params);
                });

            const auto deadline = juce::Time::currentTimeMillis() + juce::jlimit (0, 30000, getInt (params, "timeoutMs", 5000));
            juce::var lastResult;

            do
            {
                lastResult = callOnMessageThread ([this, method, &params]() {
                    return dispatch (method, params);
                });

                if (!isRetryableActionabilityError (lastResult))
                    return lastResult;

                sleepUntilReadyOrStopped (50);
            } while (juce::Time::currentTimeMillis() < deadline && !threadShouldExit());

            if (auto* errorObject = lastResult.getDynamicObject())
                return error ("operation_timeout", "Timed out waiting for actionability: " + errorObject->getProperty ("message").toString());

            return error ("operation_timeout", "Timed out waiting for actionability.");
        }

        static bool isWaitForMethod (const juce::String& method)
        {
            return method == "wait_for_ref"
                   || method == "wait_for_locator"
                   || method == "wait_for_text"
                   || method == "wait_for_value"
                   || method == "wait_for_snapshot_change";
        }

        juce::var waitForCondition (const juce::String& method, juce::DynamicObject& params)
        {
            const auto deadline = juce::Time::currentTimeMillis() + juce::jlimit (0, 30000, getInt (params, "timeoutMs", 5000));
            juce::var lastResult;

            do
            {
                lastResult = callOnMessageThread ([this, method, &params]() {
                    return evaluateWaitCondition (method, params);
                });

                if (!isError (lastResult))
                    return lastResult;

                sleepUntilReadyOrStopped (50);
            } while (juce::Time::currentTimeMillis() < deadline && !threadShouldExit());

            if (auto* errorObject = lastResult.getDynamicObject())
                return error ("operation_timeout", "Timed out waiting: " + errorObject->getProperty ("message").toString());

            return error ("operation_timeout", "Timed out waiting.");
        }

        juce::var evaluateWaitCondition (const juce::String& method, juce::DynamicObject& params)
        {
            if (method == "wait_for_ref")
            {
                auto* target = getTargetComponent (getString (params, "ref", {}));
                return target != nullptr ? snapshotAfterAction()
                                         : error ("wait_not_ready", "Ref is not available.");
            }

            if (method == "wait_for_locator")
            {
                auto result = resolveLocatorQuery (params, true, false);

                if (isError (result))
                    return result;

                auto* object = result.getDynamicObject();
                return object != nullptr && (int) object->getProperty ("count") > 0
                           ? result
                           : error ("wait_not_ready", "Locator has no matches.");
            }

            if (method == "wait_for_text")
            {
                const auto expected = getString (params, "text", {});

                if (expected.isEmpty())
                    return error ("invalid_text", "wait_for_text requires non-empty text.");

                juce::DynamicObject snapshotParams;
                snapshotParams.setProperty ("format", "json");
                snapshotParams.setProperty ("mode", "full");
                snapshotParams.setProperty ("depth", getInt (params, "depth", 12));
                auto result = snapshot (snapshotParams);
                auto* snapshotObject = result.getDynamicObject();
                auto tree = snapshotObject != nullptr ? snapshotObject->getProperty ("tree") : juce::var();

                return treeContainsText (tree, expected, (bool) params.getProperty ("exact"), params.getProperty ("visible"))
                           ? AutomationController::object ({ { "text", expected } })
                           : error ("wait_not_ready", "Text was not found.");
            }

            if (method == "wait_for_value")
            {
                auto resolution = resolveTarget (params, true, true);

                if (!resolution.error.isVoid())
                    return resolution.error;

                auto expected = getString (params, "value", {});
                auto current = semanticValueFor (*resolution.component);

                return matchesString (current, expected, (bool) params.getProperty ("exact"))
                           ? object ({ { "value", current } })
                           : error ("wait_not_ready", "Value did not match. Current value: " + current);
            }

            if (method == "wait_for_snapshot_change")
            {
                juce::DynamicObject snapshotParams;
                snapshotParams.setProperty ("format", "text");
                snapshotParams.setProperty ("depth", getInt (params, "depth", 12));
                auto result = snapshot (snapshotParams);
                auto* object = result.getDynamicObject();

                return object != nullptr && object->getProperty ("stateHash").toString() != getString (params, "stateHash", {})
                           ? result
                           : error ("wait_not_ready", "Snapshot state hash has not changed.");
            }

            return error ("unknown_method", "Unknown wait method: " + method);
        }

        static bool isAutoWaitMethod (const juce::String& method)
        {
            return method == "click"
                   || method == "dblclick"
                   || method == "right_click"
                   || method == "hover"
                   || method == "mouse_move"
                   || method == "mouse_down"
                   || method == "mouse_up"
                   || method == "wheel"
                   || method == "drag_xy"
                   || method == "drag_to"
                   || method == "drop"
                   || method == "drop_files"
                   || method == "type"
                   || method == "fill"
                   || method == "clear"
                   || method == "press"
                   || method == "key_down"
                   || method == "key_up"
                   || method == "check"
                   || method == "uncheck"
                   || method == "set_checked"
                   || method == "set_value"
                   || method == "select_option"
                   || method == "select_tab"
                   || method == "drag"
                   || method == "screenshot";
        }

        static bool isRetryableActionabilityError (const juce::var& result)
        {
            auto* resultObject = result.getDynamicObject();

            if (resultObject == nullptr || !resultObject->getProperty ("__error").isString())
                return false;

            auto code = resultObject->getProperty ("__error").toString();
            return code == "locator_not_found"
                   || code == "target_not_showing"
                   || code == "target_disabled"
                   || code == "target_empty_bounds"
                   || code == "target_not_receiving_events";
        }

        juce::var dispatch (const juce::String& method, juce::DynamicObject& params) {
            const juce::ScopedValueSetter<bool> snapshotScope(includeActionSnapshot, getBool(params, "snapshot", true));
            if (method == "ping")
                return object ({ { "status", "ok" } });

            if (method == "capabilities")
                return capabilities();

            if (method == "snapshot")
                return snapshot (params);

            if (method == "locator")
                return locator (params);

            if (method == "count")
                return count (params);

            if (method == "describe")
                return describe (params);

            if (method == "windows")
                return windows();

            if (method == "trace_start")
                return traceStart (params);

            if (method == "trace_stop")
                return traceStop();

            if (method == "screenshot")
                return screenshot (params);

            if (method == "click")
                return click (params);

            if (method == "dblclick")
                return doubleClick (params);

            if (method == "right_click")
                return rightClick (params);

            if (method == "click_xy")
                return clickXY (params);

            if (method == "hover" || method == "mouse_move")
                return mouseMove (params);

            if (method == "mouse_down")
                return mouseButton (params, true);

            if (method == "mouse_up")
                return mouseButton (params, false);

            if (method == "wheel")
                return wheel (params);

            if (method == "drag_xy")
                return dragXY (params);

            if (method == "drag_to")
                return dragTo (params);

            if (method == "drop")
                return drop (params);

            if (method == "drop_files")
                return dropFiles (params);

            if (method == "type")
                return typeText (params);

            if (method == "fill")
                return fill (params);

            if (method == "clear")
                return clear (params);

            if (method == "press")
                return pressKey (params);

            if (method == "key_down")
                return keyDown (params);

            if (method == "key_up")
                return keyUp (params);

            if (method == "check")
                return check (params, true);

            if (method == "uncheck")
                return check (params, false);

            if (method == "set_checked")
                return setChecked (params);

            if (method == "set_value")
                return setValue (params);

            if (method == "select_option")
                return selectOption (params);

            if (method == "select_tab")
                return selectTab (params);

            if (method == "drag")
                return drag (params);

            if (method == "resize_window")
                return resizeWindow (params);

            if (method == "wait")
                return wait (params);

            return error ("unknown_method", "Unknown automation method: " + method);
        }

        juce::var capabilities() const
        {
            return object ({ { "protocolVersion", protocolVersion },
                             { "session", options.sessionName },
                             { "features", object ({ { "locators", true },
                                                     { "actionability", true },
                                                     { "semanticControls", true },
                                                     { "richInput", true },
                                                     { "screenshots", true },
                                                     { "nativeScreenshots", true },
                                                     { "tracing", true },
                                                     { "windows", true },
                                                     { "windowResize", true },
                                                     { "tokenEfficientSnapshots", true },
                                                     { "snapshotModes", stringArrayToVar ({ "interesting", "full", "minimal" }) },
                                                     { "scopedSnapshots", true },
                                                     { "describe", true },
                                                     { "count", true } }) },
                             { "security", object ({ { "allowInput", options.allowInput },
                                                     { "allowMutation", options.allowMutation },
                                                     { "allowFileWrite", options.allowFileWrite },
                                                     { "artifactRoot", options.artifactRoot.getFullPathName() } }) } });
        }

        juce::var windows() const
        {
            juce::Array<juce::var> result;

            for (auto& window : automationWindows())
            {
                auto* component = window.component.getComponent();

                if (component == nullptr)
                    continue;

                result.add (object ({ { "id", window.id },
                                      { "title", componentString (component) },
                                      { "root", componentString (component) },
                                      { "class", type (*component) },
                                      { "attachedRoot", window.attachedRoot },
                                      { "focused", component->hasKeyboardFocus (true) },
                                      { "bounds", rectangleToVar (component->getScreenBounds()) } }));
            }

            return object ({ { "windows", result } });
        }

        juce::var traceStart (juce::DynamicObject& params)
        {
            auto requestedFile = getString (params, "file", {});

            if (requestedFile.isEmpty())
                requestedFile = "jucewright-trace.json";

            auto fileOrError = writableArtifactFile (requestedFile);

            if (isError (fileOrError))
                return fileOrError;

            traceFile = juce::File (fileOrError.toString());
            traceEvents.clear();
            traceEnabled = true;

            return object ({ { "trace", traceFile.getFullPathName() } });
        }

        juce::var traceStop()
        {
            traceEnabled = false;

            if (traceFile.getFullPathName().isEmpty())
                return error ("trace_not_started", "Trace has not been started.");

            auto payload = object ({ { "events", traceEvents } });
            traceFile.getParentDirectory().createDirectory();

            if (!traceFile.replaceWithText (juce::JSON::toString (payload, true)))
                return error ("trace_write_failed", "Could not write trace file: " + traceFile.getFullPathName());

            return object ({ { "trace", traceFile.getFullPathName() },
                             { "events", traceEvents.size() } });
        }
