#include <juce_core/juce_core.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if JUCE_WINDOWS
    #include <windows.h>
#endif

namespace
{
    #include "jucewright_cli/session_client.ipp"
    #include "jucewright_cli/command_options.ipp"
    #include "jucewright_cli/native_launcher.ipp"
    #include "jucewright_cli/parsing.ipp"
    #include "jucewright_cli/mcp_server.ipp"
    #include "jucewright_cli/output_help.ipp"
}

int main (int argc, char* argv[])
{
    juce::StringArray args;

    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    if (args.isEmpty() || hasFlag (args, "--help") || hasFlag (args, "-h"))
    {
        printHelp();
        return 0;
    }

    const auto sessionName = optionValue (args, "-s");
    auto command = popFront (args);

    if (command == "mcp")
    {
        runMcpServer();
        return 0;
    }

    if (command == "list")
    {
        for (auto& session : loadSessions())
        {
            if (auto* object = session.getDynamicObject())
            {
                std::cout << object->getProperty ("session").toString().toStdString()
                          << " pid=" << object->getProperty ("pid").toString().toStdString()
                          << " root=\"" << object->getProperty ("root").toString().toStdString()
                          << "\" port=" << object->getProperty ("port").toString().toStdString()
                          << "\n";
            }
        }

        return 0;
    }

    if (command == "prepare-juce-profile")
    {
        try
        {
            auto homePath = optionValue (args, "--home");
            auto sourceHomePath = optionValue (args, "--source-home", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName());
            auto appPath = optionValue (args, "--app");
            auto appName = optionValue (args, "--app-name", appNameFromPath (appPath));
            juce::StringArray extraSettings;

            for (;;)
            {
                auto setting = optionValue (args, "--copy-setting");

                if (setting.isEmpty())
                    break;

                extraSettings.addIfNotAlreadyThere (setting);
            }

            if (homePath.isEmpty())
                throw std::runtime_error ("prepare-juce-profile requires --home");

            auto result = prepareJuceProfile (juce::File::createFileWithoutCheckingPath (homePath),
                                              juce::File::createFileWithoutCheckingPath (sourceHomePath),
                                              appName,
                                              extraSettings,
                                              ! hasFlag (args, "--keep-filter-state"),
                                              ! hasFlag (args, "--keep-audio-state"));
            printResult (result, true);
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << "\n";
            return 1;
        }
    }

    if (command == "launch")
    {
        try
        {
            printResult (launchAutomationApp (args), true);
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << "\n";
            return 1;
        }
    }

    auto session = findSession (sessionName);
    auto* sessionObject = session.getDynamicObject();

    if (sessionObject == nullptr)
    {
        std::cerr << "No jucewright automation session found";

        if (sessionName.isNotEmpty())
            std::cerr << " for '" << sessionName.toStdString() << "'";

        std::cerr << ".\n";
        return 1;
    }

    try
    {
        if (command == "snapshot")
        {
            auto format = hasFlag (args, "--json") ? juce::String ("json") : optionValue (args, "--format", "text");
            auto depth = optionValue (args, "--depth", "8").getIntValue();
            auto locator = parseLocatorOptions (args);
            auto params = object ({ { "format", format }, { "depth", depth } });
            addSnapshotOptions (args, *params.getDynamicObject());
            addTimeoutOption (args, *params.getDynamicObject());
            addLocatorIfPresent (*params.getDynamicObject(), locator);
            auto result = request (*sessionObject, "snapshot", params);
            printResult (result, format == "json");
            return 0;
        }

        if (command == "locator")
        {
            auto format = optionValue (args, "--format", "json");
            auto locator = parseLocatorOptions (args);
            auto params = object ({ { "locator", locator } });
            addTimeoutOption (args, *params.getDynamicObject());
            printResult (request (*sessionObject, "locator", params), format == "json");
            return 0;
        }

        if (command == "count")
        {
            auto locator = parseLocatorOptions (args);
            auto params = object ({ { "locator", locator } });
            addTimeoutOption (args, *params.getDynamicObject());
            printResult (request (*sessionObject, "count", params), true);
            return 0;
        }

        if (command == "describe")
        {
            auto format = hasFlag (args, "--json") ? juce::String ("json") : optionValue (args, "--format", "json");
            auto depth = optionValue (args, "--depth");
            auto locator = parseLocatorOptions (args);
            auto params = object ({});
            addSnapshotOptions (args, *params.getDynamicObject());
            addTimeoutOption (args, *params.getDynamicObject());

            if (depth.isNotEmpty())
                params.getDynamicObject()->setProperty ("depth", depth.getIntValue());

            if (!locator.isVoid())
            {
                params.getDynamicObject()->setProperty ("locator", locator);
            }
            else if (!args.isEmpty())
            {
                params.getDynamicObject()->setProperty ("ref", args[0]);
            }

            auto result = request (*sessionObject, "describe", params);
            printResult (result, format == "json");
            return 0;
        }

        if (command == "capabilities")
        {
            printResult (request (*sessionObject, "capabilities", emptyObject()), true);
            return 0;
        }

        if (command == "windows")
        {
            printResult (request (*sessionObject, "windows", emptyObject()), true);
            return 0;
        }

        if (command == "trace-start")
        {
            printResult (request (*sessionObject, "trace_start", object ({ { "file", optionValue (args, "--file") } })), true);
            return 0;
        }

        if (command == "trace-stop")
        {
            printResult (request (*sessionObject, "trace_stop", emptyObject()), true);
            return 0;
        }

        if (command == "screenshot")
        {
            auto file = optionValue (args, "--file", optionValue (args, "--path"));
            auto ref = optionValue (args, "--ref");
            auto target = optionValue (args, "--target", "root");
            auto source = optionValue (args, "--source");
            auto clipX = optionValue (args, "--clip-x");
            auto clipY = optionValue (args, "--clip-y");
            auto clipW = optionValue (args, "--clip-w");
            auto clipH = optionValue (args, "--clip-h");
            auto scale = optionValue (args, "--scale");
            auto includeBase64 = hasFlag (args, "--base64");
            includeBase64 = hasFlag (args, "--no-base64") ? false : includeBase64;
            auto locator = parseLocatorOptions (args);
            auto params = object ({ { "file", file }, { "ref", ref }, { "target", target } });

            if (source.isNotEmpty()) params.getDynamicObject()->setProperty ("source", source);
            if (clipX.isNotEmpty()) params.getDynamicObject()->setProperty ("clipX", clipX.getIntValue());
            if (clipY.isNotEmpty()) params.getDynamicObject()->setProperty ("clipY", clipY.getIntValue());
            if (clipW.isNotEmpty()) params.getDynamicObject()->setProperty ("clipW", clipW.getIntValue());
            if (clipH.isNotEmpty()) params.getDynamicObject()->setProperty ("clipH", clipH.getIntValue());
            if (scale.isNotEmpty()) params.getDynamicObject()->setProperty ("scale", scale.getDoubleValue());

            params.getDynamicObject()->setProperty ("includeBase64", includeBase64);
            addActionOptions (args, *params.getDynamicObject());
            addLocatorIfPresent (*params.getDynamicObject(), locator);
            auto result = request (*sessionObject, "screenshot", params);
            printResult (result);
            return 0;
        }

        if (command == "click" || command == "dblclick" || command == "right-click")
        {
            auto button = optionValue (args, "--button");
            auto clickCount = optionValue (args, "--click-count");
            auto position = optionValue (args, "--position");
            auto menuItem = optionValue (args, "--menu-item");
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : (args.size() >= 1 ? args[0] : juce::String());
            auto params = object ({ { "ref", ref } });

            if (button.isNotEmpty())
                params.getDynamicObject()->setProperty ("button", button);

            if (clickCount.isNotEmpty())
                params.getDynamicObject()->setProperty ("clickCount", clickCount.getIntValue());

            if (menuItem.isNotEmpty())
                params.getDynamicObject()->setProperty ("menuItem", menuItem);

            addPositionIfPresent (*params.getDynamicObject(), position);
            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject,
                                  command == "click" ? "click" : (command == "dblclick" ? "dblclick" : "right_click"),
                                  params));
            return 0;
        }

        if (command == "click-xy" && args.size() >= 2)
        {
            auto target = optionValue (args, "--target", "root");
            printResult (request (*sessionObject, "click_xy", object ({ { "target", target }, { "x", args[0].getIntValue() }, { "y", args[1].getIntValue() } })));
            return 0;
        }

        if ((command == "hover" || command == "mouse-move") && args.size() >= 2)
        {
            auto target = optionValue (args, "--target", "root");
            printResult (request (*sessionObject, command == "hover" ? "hover" : "mouse_move", object ({ { "target", target }, { "x", args[0].getIntValue() }, { "y", args[1].getIntValue() } })));
            return 0;
        }

        if ((command == "mouse-down" || command == "mouse-up") && args.size() >= 2)
        {
            auto target = optionValue (args, "--target", "root");
            printResult (request (*sessionObject, command == "mouse-down" ? "mouse_down" : "mouse_up", object ({ { "target", target }, { "x", args[0].getIntValue() }, { "y", args[1].getIntValue() } })));
            return 0;
        }

        if (command == "wheel" && args.size() >= 2)
        {
            auto target = optionValue (args, "--target", "root");
            auto dx = optionValue (args, "--dx", "0").getDoubleValue();
            auto dy = optionValue (args, "--dy", "0").getDoubleValue();
            printResult (request (*sessionObject, "wheel", object ({ { "target", target }, { "x", args[0].getIntValue() }, { "y", args[1].getIntValue() }, { "deltaX", dx }, { "deltaY", dy } })));
            return 0;
        }

        if (command == "drag-xy" && args.size() >= 4)
        {
            auto target = optionValue (args, "--target", "root");
            auto steps = optionValue (args, "--steps");
            auto params = object ({ { "target", target },
                                    { "x", args[0].getIntValue() },
                                    { "y", args[1].getIntValue() },
                                    { "toX", args[2].getIntValue() },
                                    { "toY", args[3].getIntValue() } });

            if (steps.isNotEmpty())
                params.getDynamicObject()->setProperty ("steps", steps.getIntValue());

            printResult (request (*sessionObject, "drag_xy", params));
            return 0;
        }

        if (command == "type")
        {
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : popFront (args);
            auto params = object ({ { "ref", ref }, { "text", args.joinIntoString (" ") } });
            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject, "type", params));
            return 0;
        }

        if (command == "fill" || command == "clear")
        {
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : popFront (args);
            auto params = object ({ { "ref", ref }, { "text", command == "fill" ? args.joinIntoString (" ") : juce::String() } });
            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject, command == "fill" ? "fill" : "clear", params));
            return 0;
        }

        if (command == "check" || command == "uncheck" || command == "set-checked")
        {
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : popFront (args);
            auto params = object ({ { "ref", ref } });

            if (command == "set-checked")
            {
                if (args.isEmpty() || (args[0] != "true" && args[0] != "false"))
                    throw std::runtime_error ("set-checked requires true or false");

                params.getDynamicObject()->setProperty ("checked", args[0] == "true");
            }

            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject,
                                  command == "check" ? "check" : (command == "uncheck" ? "uncheck" : "set_checked"),
                                  params));
            return 0;
        }

        if (command == "set-value")
        {
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : popFront (args);

            if (args.isEmpty())
                throw std::runtime_error ("set-value requires a value");

            auto params = object ({ { "ref", ref }, { "value", parseValue (args[0]) } });
            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject, "set_value", params));
            return 0;
        }

        if (command == "select-option")
        {
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto text = optionValue (args, "--text");
            auto index = optionValue (args, "--index");
            auto id = optionValue (args, "--id");
            auto menuItem = optionValue (args, "--menu-item");
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : (args.size() >= 1 ? args[0] : juce::String());
            auto params = object ({ { "ref", ref },
                                    { "text", text } });

            if (index.isNotEmpty())
                params.getDynamicObject()->setProperty ("index", index.getIntValue());

            if (id.isNotEmpty())
                params.getDynamicObject()->setProperty ("id", id.getIntValue());

            if (menuItem.isNotEmpty())
                params.getDynamicObject()->setProperty ("menuItem", menuItem);

            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject, "select_option", params));
            return 0;
        }

        if (command == "select-tab")
        {
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto name = optionValue (args, "--name");
            auto index = optionValue (args, "--index");
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : (args.size() >= 1 ? args[0] : juce::String());
            auto params = object ({ { "ref", ref },
                                    { "name", name } });

            if (index.isNotEmpty())
                params.getDynamicObject()->setProperty ("index", index.getIntValue());

            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject, "select_tab", params));
            return 0;
        }

        if ((command == "press" || command == "key-down" || command == "key-up") && args.size() >= 1)
        {
            auto ref = optionValue (args, "--ref");
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto params = object ({ { "key", args[0] }, { "ref", ref } });
            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject,
                                  command == "press" ? "press" : (command == "key-down" ? "key_down" : "key_up"),
                                  params));
            return 0;
        }

        if (command == "drag-to")
        {
            auto steps = optionValue (args, "--steps");
            auto targetRef = optionValue (args, "--target-ref");
            auto targetLocator = parseTargetLocatorOptions (args);
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto sourceRef = !locator.isVoid() ? juce::String() : popFront (args);

            if (targetRef.isEmpty() && targetLocator.isVoid() && !args.isEmpty())
                targetRef = popFront (args);

            auto params = object ({ { "ref", sourceRef }, { "targetRef", targetRef } });

            if (steps.isNotEmpty())
                params.getDynamicObject()->setProperty ("steps", steps.getIntValue());

            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);

            if (!targetLocator.isVoid())
                params.getDynamicObject()->setProperty ("targetLocator", targetLocator);

            printResult (request (*sessionObject, "drag_to", params));
            return 0;
        }

        if (command == "drop")
        {
            auto description = optionValue (args, "--description");
            auto position = optionValue (args, "--position");
            auto sourceRef = optionValue (args, "--source-ref");
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : popFront (args);

            if (description.isEmpty())
                throw std::runtime_error ("drop requires --description");

            auto params = object ({ { "ref", ref }, { "description", description } });
            addPositionIfPresent (*params.getDynamicObject(), position);

            if (sourceRef.isNotEmpty())
                params.getDynamicObject()->setProperty ("sourceRef", sourceRef);

            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject, "drop", params));
            return 0;
        }

        if (command == "drop-files")
        {
            juce::Array<juce::var> files;

            for (;;)
            {
                auto file = optionValue (args, "--file");

                if (file.isEmpty())
                    break;

                files.add (file);
            }

            auto filesList = optionValue (args, "--files");
            if (filesList.isNotEmpty())
            {
                juce::StringArray parsed;
                parsed.addTokens (filesList, ",", "\"'");
                parsed.trim();
                parsed.removeEmptyStrings();

                for (const auto& file : parsed)
                    files.add (file);
            }

            auto position = optionValue (args, "--position");
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : popFront (args);

            if (files.isEmpty())
                throw std::runtime_error ("drop-files requires --file or --files");

            auto params = object ({ { "ref", ref }, { "files", juce::var (files) } });
            addPositionIfPresent (*params.getDynamicObject(), position);
            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject, "drop_files", params));
            return 0;
        }

        if (command == "drag")
        {
            auto dx = optionValue (args, "--dx", "0").getIntValue();
            auto dy = optionValue (args, "--dy", "0").getIntValue();
            auto steps = optionValue (args, "--steps");
            auto position = optionValue (args, "--position");
            juce::DynamicObject tempParams;
            addActionOptions (args, tempParams);
            auto locator = parseLocatorOptions (args);
            auto ref = !locator.isVoid() ? juce::String() : (args.size() >= 1 ? args[0] : juce::String());
            auto params = object ({ { "ref", ref }, { "dx", dx }, { "dy", dy } });

            if (steps.isNotEmpty())
                params.getDynamicObject()->setProperty ("steps", steps.getIntValue());

            addPositionIfPresent (*params.getDynamicObject(), position);
            addActionOptionsAndLocator (*params.getDynamicObject(), tempParams, locator);
            printResult (request (*sessionObject, "drag", params));
            return 0;
        }

        if (command == "resize-window")
        {
            auto w = optionValue (args, "--w");
            if (w.isEmpty())
                w = optionValue (args, "--width");

            auto h = optionValue (args, "--h");
            if (h.isEmpty())
                h = optionValue (args, "--height");

            auto target = optionValue (args, "--target");

            if (target.isEmpty() && args.size() >= 1 && ! args[0].startsWithChar ('-'))
                target = args[0];

            if (target.isEmpty())
                target = "root";

            if (w.isEmpty() || h.isEmpty())
                throw std::runtime_error ("resize-window requires --w/--width and --h/--height");

            printResult (request (*sessionObject, "resize_window", object ({ { "target", target },
                                                                              { "w", w.getIntValue() },
                                                                              { "h", h.getIntValue() } })));
            return 0;
        }

        if (command == "wait")
        {
            printResult (request (*sessionObject, "wait", object ({ { "ms", optionValue (args, "--ms", "250").getIntValue() } })));
            return 0;
        }

        if (command == "wait-for-ref")
        {
            printResult (request (*sessionObject, "wait_for_ref", object ({ { "ref", args.size() >= 1 ? args[0] : juce::String() },
                                                                            { "timeoutMs", optionValue (args, "--timeout-ms", "5000").getIntValue() } })));
            return 0;
        }

        if (command == "wait-for-locator")
        {
            auto locator = parseLocatorOptions (args);
            auto params = object ({ { "locator", locator },
                                    { "timeoutMs", optionValue (args, "--timeout-ms", "5000").getIntValue() } });
            printResult (request (*sessionObject, "wait_for_locator", params), true);
            return 0;
        }

        if (command == "wait-for-text")
        {
            auto timeout = optionValue (args, "--timeout-ms", "5000").getIntValue();
            auto depth = optionValue (args, "--depth");
            auto exact = hasFlag (args, "--exact");
            auto hidden = hasFlag (args, "--hidden");
            auto visible = hasFlag (args, "--visible");
            auto params = object ({ { "text", args.joinIntoString (" ") },
                                    { "timeoutMs", timeout } });

            if (depth.isNotEmpty())
                params.getDynamicObject()->setProperty ("depth", depth.getIntValue());

            if (exact)
                params.getDynamicObject()->setProperty ("exact", true);

            if (hidden)
                params.getDynamicObject()->setProperty ("visible", false);
            else if (visible)
                params.getDynamicObject()->setProperty ("visible", true);

            printResult (request (*sessionObject, "wait_for_text", params));
            return 0;
        }

        if (command == "wait-for-value")
        {
            auto value = optionValue (args, "--value");
            auto ref = optionValue (args, "--ref");
            auto locator = parseLocatorOptions (args);
            auto params = object ({ { "ref", ref },
                                    { "value", value },
                                    { "timeoutMs", optionValue (args, "--timeout-ms", "5000").getIntValue() } });
            addLocatorIfPresent (*params.getDynamicObject(), locator);
            printResult (request (*sessionObject, "wait_for_value", params), true);
            return 0;
        }

        if (command == "wait-for-snapshot-change")
        {
            printResult (request (*sessionObject,
                                  "wait_for_snapshot_change",
                                  object ({ { "stateHash", optionValue (args, "--state-hash") },
                                            { "timeoutMs", optionValue (args, "--timeout-ms", "5000").getIntValue() } })));
            return 0;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }

    printHelp();
    return 1;
}
