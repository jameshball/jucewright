        void exerciseSettings()
        {
            runCli ({ "-s", sessionName, "select-option", "--role", "comboBox", "--value", "LookAndFeel_V4 (Dark)", "--exact", "--text", "LookAndFeel_V4 (Light)" });
            require ((int) asObject (readLocator ({ "--role", "comboBox", "--value", "LookAndFeel_V4 (Light)", "--exact" }),
                                     "Settings LookAndFeel locator").getProperty ("count") >= 1,
                     "Settings LookAndFeel combo did not update");
            captureScreenshot ("settings-light.png");
        }

        juce::File captureScreenshot (const juce::String& name, std::initializer_list<juce::String> screenshotArgs = {})
        {
            runCli ({ "-s", sessionName, "wait", "--ms", "250" });
            auto args = makeArgs ({ "-s", sessionName, "screenshot", "--file", name, "--no-base64" });
            args.addArray (makeArgs (screenshotArgs));

            auto output = runCli (args);
            juce::String outputPath;

            for (auto line : juce::StringArray::fromLines (output))
            {
                auto trimmed = line.trim();

                if (trimmed.isNotEmpty())
                {
                    outputPath = trimmed;
                    break;
                }
            }

            auto screenshot = juce::File (outputPath);
            require (screenshot.existsAsFile() && screenshot.getSize() > 1000,
                     "Screenshot was not written or is too small: " + outputPath);
            copyEvidenceFile (outputPath, name);
            return evidenceDirectory.getChildFile (name);
        }

        void assertScreenshotHasVariation (const juce::File& file,
                                           const juce::String& label,
                                           int minimumUniqueColours,
                                           int minimumLuminanceRange)
        {
            auto image = juce::ImageFileFormat::loadFrom (file);
            require (!image.isNull(), label + " could not be decoded: " + file.getFullPathName());

            std::set<int> colours;
            auto minLuminance = 255;
            auto maxLuminance = 0;
            auto samples = 0;
            const auto stepX = juce::jmax (1, image.getWidth() / 32);
            const auto stepY = juce::jmax (1, image.getHeight() / 32);

            for (int y = 0; y < image.getHeight(); y += stepY)
            {
                for (int x = 0; x < image.getWidth(); x += stepX)
                {
                    const auto colour = image.getPixelAt (x, y);

                    if (colour.getAlpha() == 0)
                        continue;

                    const auto red = (int) colour.getRed();
                    const auto green = (int) colour.getGreen();
                    const auto blue = (int) colour.getBlue();
                    const auto luminance = (red * 2126 + green * 7152 + blue * 722) / 10000;

                    colours.insert ((red << 16) | (green << 8) | blue);
                    minLuminance = juce::jmin (minLuminance, luminance);
                    maxLuminance = juce::jmax (maxLuminance, luminance);
                    ++samples;
                }
            }

            require (samples > 20, label + " did not contain enough opaque pixels");

            const auto luminanceRange = maxLuminance - minLuminance;
            require ((int) colours.size() >= minimumUniqueColours && luminanceRange >= minimumLuminanceRange,
                     label + " looks blank or too flat: uniqueColours=" + juce::String ((int) colours.size())
                         + " luminanceRange=" + juce::String (luminanceRange));
        }

        void assertScreenshotsDiffer (const juce::File& before,
                                      const juce::File& after,
                                      const juce::String& label,
                                      int minimumDifferentSamples = 12)
        {
            auto beforeImage = juce::ImageFileFormat::loadFrom (before);
            auto afterImage = juce::ImageFileFormat::loadFrom (after);

            require (!beforeImage.isNull(), label + " before image could not be decoded: " + before.getFullPathName());
            require (!afterImage.isNull(), label + " after image could not be decoded: " + after.getFullPathName());

            if (beforeImage.getBounds() != afterImage.getBounds())
                return;

            auto differentSamples = 0;
            const auto stepX = juce::jmax (1, beforeImage.getWidth() / 160);
            const auto stepY = juce::jmax (1, beforeImage.getHeight() / 120);

            for (int y = 0; y < beforeImage.getHeight(); y += stepY)
            {
                for (int x = 0; x < beforeImage.getWidth(); x += stepX)
                {
                    if (beforeImage.getPixelAt (x, y) != afterImage.getPixelAt (x, y))
                    {
                        ++differentSamples;

                        if (differentSamples >= minimumDifferentSamples)
                            return;
                    }
                }
            }

            require (false, label + " did not visibly change enough: differentSamples=" + juce::String (differentSamples));
        }

        void copyEvidenceFile (const juce::String& sourcePath, const juce::String& evidenceName)
        {
            auto source = juce::File (sourcePath);
            auto destination = evidenceDirectory.getChildFile (evidenceName);
            destination.deleteFile();
            require (source.copyFileTo (destination), "Could not copy evidence file: " + source.getFullPathName());
        }

        juce::String runMcpBatch (std::initializer_list<juce::String> requests)
        {
            auto requestFile = tempDirectory().getNonexistentChildFile ("jucewright-demorunner-mcp", ".jsonl");
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

            auto output = runProcess (command, "jucewright mcp", true, 15000).output;
            requestFile.deleteFile();
            return output;
        }

        juce::var parseMcpLine (const juce::StringArray& lines, int index)
        {
            require (juce::isPositiveAndBelow (index, lines.size()), "MCP response " + juce::String (index) + " missing");
            auto parsed = juce::JSON::parse (lines[index]);
            asObject (parsed, "MCP response " + juce::String (index));
            return parsed;
        }

        juce::var assertMcpResult (const juce::var& response, int expectedId)
        {
            auto& responseObject = asObject (response, "MCP response");
            require ((int) responseObject.getProperty ("id") == expectedId,
                     "MCP response id mismatch: " + juce::JSON::toString (response, true));
            require (responseObject.getProperty ("error").isVoid(),
                     "MCP returned an error: " + juce::JSON::toString (response, true));
            return responseObject.getProperty ("result");
        }

        void runMcpSmoke()
        {
            auto output = runMcpBatch ({
                R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25"}})",
                R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})",
                R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"juce_snapshot","arguments":{"session":"juce_demorunner","depth":5}}})",
                R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"juce_screenshot","arguments":{"session":"juce_demorunner","target":"root","includeBase64":true}}})"
            });

            auto rawLines = juce::StringArray::fromLines (output);
            juce::StringArray lines;

            for (auto line : rawLines)
                if (line.trim().startsWithChar ('{'))
                    lines.add (line);

            require (lines.size() >= 4, "MCP E2E expected at least 4 response lines, got " + juce::String (lines.size()));

            assertMcpResult (parseMcpLine (lines, 0), 1);

            auto toolsList = assertMcpResult (parseMcpLine (lines, 1), 2);
            auto tools = asObject (toolsList, "MCP tools/list").getProperty ("tools");
            require (tools.isArray() && tools.getArray()->size() >= 10, "MCP tools/list returned too few tools");

            auto snapshotResult = assertMcpResult (parseMcpLine (lines, 2), 3);
            auto snapshotContent = asObject (snapshotResult, "MCP snapshot").getProperty ("content");
            require (snapshotContent.isArray() && !snapshotContent.getArray()->isEmpty(),
                     "MCP snapshot did not return content");
            auto snapshotText = asObject (snapshotContent.getArray()->getReference (0), "MCP snapshot content").getProperty ("text").toString();
            require (snapshotText.contains ("JUCE Logo"),
                     "MCP snapshot did not include JUCE Logo");
            auto parsedSnapshot = juce::JSON::parse (snapshotText);
            require (asObject (parsedSnapshot, "MCP parsed snapshot").getProperty ("mode").toString() == "interesting",
                     "MCP DemoRunner snapshot should default to interesting JSON");

            auto screenshotResult = assertMcpResult (parseMcpLine (lines, 3), 4);
            auto screenshotContent = asObject (screenshotResult, "MCP screenshot").getProperty ("content");
            require (screenshotContent.isArray(), "MCP screenshot did not return content");

            bool foundImage = false;

            for (const auto& item : *screenshotContent.getArray())
            {
                auto& contentItem = asObject (item, "MCP screenshot content");
                foundImage = foundImage
                             || (contentItem.getProperty ("type").toString() == "image"
                                 && contentItem.getProperty ("mimeType").toString() == "image/png"
                                 && contentItem.getProperty ("data").toString().length() > 1000);
            }

            require (foundImage, "MCP screenshot did not return PNG image content");
        }
