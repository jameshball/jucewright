    juce::File supportDirectoryForHome (const juce::File& home)
    {
#if JUCE_MAC
        return home.getChildFile ("Library").getChildFile ("Application Support");
#elif JUCE_WINDOWS
        return home.getChildFile ("AppData").getChildFile ("Roaming");
#else
        return home.getChildFile (".config");
#endif
    }

#if JUCE_WINDOWS
    juce::File windowsRoamingDataDirectoryForHome (const juce::File& home)
    {
        return home.getChildFile ("AppData").getChildFile ("Roaming");
    }

    juce::File windowsLocalDataDirectoryForHome (const juce::File& home)
    {
        return home.getChildFile ("AppData").getChildFile ("Local");
    }
#elif ! JUCE_MAC
    juce::File linuxConfigDirectoryForHome (const juce::File& home)
    {
        return home.getChildFile (".config");
    }

    juce::File linuxDataDirectoryForHome (const juce::File& home)
    {
        return home.getChildFile (".local").getChildFile ("share");
    }

    juce::File linuxCacheDirectoryForHome (const juce::File& home)
    {
        return home.getChildFile (".cache");
    }
#endif

    juce::StringArray automationEnvironmentForLaunch (const juce::File& home,
                                                      const juce::File& artifactDir,
                                                      const juce::String& sessionName)
    {
        juce::StringArray environment;

        auto add = [&] (const juce::String& name, const juce::String& value)
        {
            environment.add (name + "=" + value);
        };

        add ("JUCEWRIGHT_AUTOMATION", "1");
        add ("JUCEWRIGHT_SESSION", sessionName);
        add ("JUCEWRIGHT_ARTIFACT_ROOT", artifactDir.getFullPathName());

#if JUCE_MAC
        add ("HOME", home.getFullPathName());
        add ("CFFIXED_USER_HOME", home.getFullPathName());
#elif JUCE_WINDOWS
        auto roaming = windowsRoamingDataDirectoryForHome (home);
        auto local = windowsLocalDataDirectoryForHome (home);
        roaming.createDirectory();
        local.createDirectory();

        add ("USERPROFILE", home.getFullPathName());
        add ("HOME", home.getFullPathName());
        add ("APPDATA", roaming.getFullPathName());
        add ("LOCALAPPDATA", local.getFullPathName());

        auto homePath = home.getFullPathName();
        if (homePath.length() >= 2 && homePath[1] == ':')
        {
            add ("HOMEDRIVE", homePath.substring (0, 2));
            add ("HOMEPATH", homePath.substring (2));
        }
#else
        auto config = linuxConfigDirectoryForHome (home);
        auto data = linuxDataDirectoryForHome (home);
        auto cache = linuxCacheDirectoryForHome (home);
        config.createDirectory();
        data.createDirectory();
        cache.createDirectory();

        add ("HOME", home.getFullPathName());
        add ("XDG_CONFIG_HOME", config.getFullPathName());
        add ("XDG_DATA_HOME", data.getFullPathName());
        add ("XDG_CACHE_HOME", cache.getFullPathName());
#endif

        return environment;
    }

    void addEnvironmentToCommand (juce::StringArray& command, const juce::StringArray& environment)
    {
        for (const auto& assignment : environment)
            command.add (assignment);
    }

#if JUCE_WINDOWS
    void appendBackslashes (juce::String& text, int count)
    {
        while (count-- > 0)
            text += "\\";
    }

    juce::String quoteWindowsCommandLineArgument (const juce::String& argument)
    {
        if (argument.isNotEmpty() && ! argument.containsAnyOf (" \t\n\v\""))
            return argument;

        juce::String quoted = "\"";
        int backslashes = 0;

        for (auto character = argument.getCharPointer(); ! character.isEmpty();)
        {
            auto c = character.getAndAdvance();

            if (c == '\\')
            {
                ++backslashes;
                continue;
            }

            if (c == '"')
            {
                appendBackslashes (quoted, backslashes * 2 + 1);
                quoted += "\"";
                backslashes = 0;
                continue;
            }

            appendBackslashes (quoted, backslashes);
            backslashes = 0;
            quoted += juce::String::charToString (c);
        }

        appendBackslashes (quoted, backslashes * 2);
        quoted += "\"";
        return quoted;
    }

    juce::String windowsCommandLineForApp (const juce::File& app, const juce::StringArray& appArgs)
    {
        juce::StringArray arguments;
        arguments.add (app.getFullPathName());
        arguments.addArray (appArgs);

        juce::StringArray quoted;
        for (const auto& argument : arguments)
            quoted.add (quoteWindowsCommandLineArgument (argument));

        return quoted.joinIntoString (" ");
    }

    juce::String windowsLastErrorMessage (const juce::String& context)
    {
        const auto error = GetLastError();
        LPWSTR message = nullptr;

        FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        nullptr,
                        error,
                        MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                        reinterpret_cast<LPWSTR> (&message),
                        0,
                        nullptr);

        juce::String result = context + " failed";
        if (message != nullptr)
        {
            result += ": ";
            result += juce::String (message).trim();
            LocalFree (message);
        }

        result += " (";
        result += juce::String ((int) error);
        result += ")";
        return result;
    }

    std::vector<wchar_t> wideNullTerminatedString (const juce::String& text)
    {
        std::vector<wchar_t> result;

        for (auto character = text.toWideCharPointer(); ! character.isEmpty();)
            result.push_back ((wchar_t) character.getAndAdvance());

        result.push_back (L'\0');
        return result;
    }

    juce::String environmentAssignmentName (const juce::String& assignment)
    {
        return assignment.upToFirstOccurrenceOf ("=", false, false);
    }

    std::vector<wchar_t> windowsEnvironmentBlock (const juce::StringArray& overrides)
    {
        juce::StringArray entries;

        if (auto* environment = GetEnvironmentStringsW())
        {
            for (auto* entry = environment; *entry != L'\0'; entry += wcslen (entry) + 1)
                entries.add (juce::String (entry));

            FreeEnvironmentStringsW (environment);
        }

        for (const auto& override : overrides)
        {
            auto name = environmentAssignmentName (override);

            for (int i = entries.size(); --i >= 0;)
                if (environmentAssignmentName (entries[i]).equalsIgnoreCase (name))
                    entries.remove (i);

            entries.add (override);
        }

        entries.sort (true);

        std::vector<wchar_t> block;
        for (const auto& entry : entries)
        {
            auto wide = wideNullTerminatedString (entry);
            block.insert (block.end(), wide.begin(), wide.end());
        }

        block.push_back (L'\0');
        return block;
    }

    HANDLE createWindowsLogFile (const juce::File& file)
    {
        SECURITY_ATTRIBUTES attributes {};
        attributes.nLength = sizeof (attributes);
        attributes.bInheritHandle = TRUE;

        auto path = wideNullTerminatedString (file.getFullPathName());
        return CreateFileW (path.data(),
                            GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            &attributes,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,
                            nullptr);
    }

    void launchWindowsApp (const juce::File& app,
                           const juce::StringArray& appArgs,
                           const juce::StringArray& environment,
                           const juce::File& stdoutFile,
                           const juce::File& stderrFile)
    {
        auto stdoutHandle = createWindowsLogFile (stdoutFile);
        if (stdoutHandle == INVALID_HANDLE_VALUE)
            throw std::runtime_error (windowsLastErrorMessage ("CreateFile stdout").toStdString());

        auto stderrHandle = createWindowsLogFile (stderrFile);
        if (stderrHandle == INVALID_HANDLE_VALUE)
        {
            CloseHandle (stdoutHandle);
            throw std::runtime_error (windowsLastErrorMessage ("CreateFile stderr").toStdString());
        }

        STARTUPINFOW startupInfo {};
        startupInfo.cb = sizeof (startupInfo);
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdInput = GetStdHandle (STD_INPUT_HANDLE);
        startupInfo.hStdOutput = stdoutHandle;
        startupInfo.hStdError = stderrHandle;

        PROCESS_INFORMATION processInfo {};
        auto commandLine = wideNullTerminatedString (windowsCommandLineForApp (app, appArgs));
        auto environmentBlock = windowsEnvironmentBlock (environment);
        auto workingDirectory = wideNullTerminatedString (app.getParentDirectory().getFullPathName());

        auto launched = CreateProcessW (nullptr,
                                        commandLine.data(),
                                        nullptr,
                                        nullptr,
                                        TRUE,
                                        CREATE_UNICODE_ENVIRONMENT,
                                        environmentBlock.data(),
                                        workingDirectory.data(),
                                        &startupInfo,
                                        &processInfo);

        CloseHandle (stdoutHandle);
        CloseHandle (stderrHandle);

        if (! launched)
            throw std::runtime_error (windowsLastErrorMessage ("CreateProcess").toStdString());

        CloseHandle (processInfo.hThread);
        CloseHandle (processInfo.hProcess);
    }
#endif

    juce::String appNameFromPath (const juce::String& path)
    {
        if (path.isEmpty())
            return {};

        auto file = juce::File::createFileWithoutCheckingPath (path);
        auto current = file;

        while (current != juce::File())
        {
            if (current.getFileExtension().equalsIgnoreCase (".app"))
                return current.getFileNameWithoutExtension();

            auto parent = current.getParentDirectory();

            if (parent == current)
                break;

            current = parent;
        }

        return file.getFileNameWithoutExtension();
    }

    juce::File macBundleForAppPath (const juce::File& app)
    {
        auto current = app;

        while (current != juce::File())
        {
            if (current.getFileExtension().equalsIgnoreCase (".app") && current.isDirectory())
                return current;

            auto parent = current.getParentDirectory();

            if (parent == current)
                break;

            current = parent;
        }

        return {};
    }

    juce::XmlElement* findPropertyValue (juce::XmlElement& root, const juce::String& name)
    {
        for (auto* child : root.getChildIterator())
            if (child != nullptr && child->hasTagName ("VALUE") && child->getStringAttribute ("name") == name)
                return child;

        return nullptr;
    }

    void removePropertyValues (juce::XmlElement& root, const juce::String& name)
    {
        for (int i = root.getNumChildElements(); --i >= 0;)
        {
            auto* child = root.getChildElement (i);

            if (child != nullptr && child->hasTagName ("VALUE") && child->getStringAttribute ("name") == name)
                root.removeChildElement (child, true);
        }
    }

    juce::XmlElement& getOrCreatePropertyValue (juce::XmlElement& root, const juce::String& name)
    {
        if (auto* existing = findPropertyValue (root, name))
            return *existing;

        auto* value = root.createNewChildElement ("VALUE");
        value->setAttribute ("name", name);
        return *value;
    }

    void disableJuceAudioRestore (juce::XmlElement& root)
    {
        auto& value = getOrCreatePropertyValue (root, "audioSetup");
        value.deleteAllChildElements();

        auto* setup = value.createNewChildElement ("DEVICESETUP");
        setup->setAttribute ("deviceType", "");
        setup->setAttribute ("audioOutputDeviceName", "");
        setup->setAttribute ("audioInputDeviceName", "");
        setup->setAttribute ("audioDeviceInChans", "0");
        setup->setAttribute ("audioDeviceOutChans", "0");
    }

    void copySettingFileIfPresent (const juce::File& sourceSupport,
                                   const juce::File& targetSupport,
                                   const juce::String& fileName,
                                   juce::StringArray& copied)
    {
        if (fileName.isEmpty())
            return;

        auto source = sourceSupport.getChildFile (fileName);

        if (! source.existsAsFile())
            return;

        auto target = targetSupport.getChildFile (fileName);
        target.deleteFile();

        if (! source.copyFileTo (target))
            throw std::runtime_error (("Could not copy settings file to " + target.getFullPathName()).toStdString());

        copied.addIfNotAlreadyThere (target.getFullPathName());
    }

    juce::var prepareJuceProfile (const juce::File& home,
                                  const juce::File& sourceHome,
                                  const juce::String& appName,
                                  const juce::StringArray& extraSettings,
                                  bool clearFilterState,
                                  bool disableAudioRestore)
    {
        if (appName.isEmpty())
            throw std::runtime_error ("prepare-juce-profile requires --app-name or --app");

        auto sourceSupport = supportDirectoryForHome (sourceHome);
        auto targetSupport = supportDirectoryForHome (home);
        juce::StringArray copied;

        if (! targetSupport.createDirectory())
            throw std::runtime_error (("Could not create profile support directory " + targetSupport.getFullPathName()).toStdString());

        if (sourceSupport.isDirectory())
        {
            for (const auto& entry : juce::RangedDirectoryIterator (sourceSupport, false, "*.settings", juce::File::findFiles))
            {
                auto file = entry.getFile();
                auto fileName = file.getFileName();

                if (! fileName.matchesWildcard (appName + "*.settings", false))
                    continue;

                auto target = targetSupport.getChildFile (fileName);
                target.deleteFile();

                if (! file.copyFileTo (target))
                    throw std::runtime_error (("Could not copy settings file to " + target.getFullPathName()).toStdString());

                copied.addIfNotAlreadyThere (target.getFullPathName());
            }

            for (const auto& setting : extraSettings)
                copySettingFileIfPresent (sourceSupport, targetSupport, setting, copied);
        }

        auto mainSettings = targetSupport.getChildFile (appName + ".settings");
        std::unique_ptr<juce::XmlElement> root;

        if (mainSettings.existsAsFile())
            root = juce::parseXML (mainSettings);

        if (root == nullptr || ! root->hasTagName ("PROPERTIES"))
            root = std::make_unique<juce::XmlElement> ("PROPERTIES");

        if (clearFilterState)
            removePropertyValues (*root, "filterState");

        if (disableAudioRestore)
            disableJuceAudioRestore (*root);

        if (! mainSettings.replaceWithText (root->toString()))
            throw std::runtime_error (("Could not write settings file " + mainSettings.getFullPathName()).toStdString());

        copied.addIfNotAlreadyThere (mainSettings.getFullPathName());

        juce::Array<juce::var> copiedFiles;
        for (const auto& file : copied)
            copiedFiles.add (file);

        auto* result = new juce::DynamicObject();
        result->setProperty ("home", home.getFullPathName());
        result->setProperty ("supportDirectory", targetSupport.getFullPathName());
        result->setProperty ("appName", appName);
        result->setProperty ("settingsFile", mainSettings.getFullPathName());
        result->setProperty ("copied", juce::var (copiedFiles));
        result->setProperty ("clearedFilterState", clearFilterState);
        result->setProperty ("disabledAudioRestore", disableAudioRestore);
        return juce::var (result);
    }

    juce::var waitForSession (const juce::String& sessionName, int timeoutMs)
    {
        const auto deadline = juce::Time::currentTimeMillis() + juce::jmax (1, timeoutMs);

        while (juce::Time::currentTimeMillis() < deadline)
        {
            auto session = findSession (sessionName);

            if (session.getDynamicObject() != nullptr)
                return session;

            juce::Thread::sleep (250);
        }

        return {};
    }

    void addRepeatedOptionValues (juce::StringArray& values, juce::StringArray& args, const juce::String& option)
    {
        for (;;)
        {
            auto value = optionValue (args, option);

            if (value.isEmpty())
                break;

            values.addIfNotAlreadyThere (value);
        }
    }

    juce::StringArray remainingArgsAfterDoubleDash (juce::StringArray& args)
    {
        juce::StringArray result;
        auto separator = args.indexOf ("--");

        if (separator < 0)
            return result;

        for (int i = separator + 1; i < args.size(); ++i)
            result.add (args[i]);

        while (args.size() > separator)
            args.remove (separator);

        return result;
    }

    juce::var launchAutomationApp (juce::StringArray& args)
    {
        auto appPath = optionValue (args, "--app");
        auto appName = optionValue (args, "--app-name", appNameFromPath (appPath));
        auto sessionName = optionValue (args, "--session", appName);
        auto artifactPath = optionValue (args, "--artifact-dir",
                                         juce::File::getSpecialLocation (juce::File::tempDirectory)
                                             .getChildFile (sessionName + "-jucewright")
                                             .getFullPathName());
        auto homePath = optionValue (args, "--home", juce::File::createFileWithoutCheckingPath (artifactPath).getChildFile ("home").getFullPathName());
        auto sourceHomePath = optionValue (args, "--source-home", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName());
        auto stdoutPath = optionValue (args, "--stdout", juce::File::createFileWithoutCheckingPath (artifactPath).getChildFile ("app.stdout.log").getFullPathName());
        auto stderrPath = optionValue (args, "--stderr", juce::File::createFileWithoutCheckingPath (artifactPath).getChildFile ("app.stderr.log").getFullPathName());
        auto timeoutMs = optionValue (args, "--timeout-ms", "30000").getIntValue();
        auto noWait = hasFlag (args, "--no-wait");
        auto noProfile = hasFlag (args, "--no-profile");
        auto keepFilterState = hasFlag (args, "--keep-filter-state");
        auto keepAudioState = hasFlag (args, "--keep-audio-state");
        juce::StringArray extraSettings;
        addRepeatedOptionValues (extraSettings, args, "--copy-setting");
        auto appArgs = remainingArgsAfterDoubleDash (args);

        if (appPath.isEmpty())
            throw std::runtime_error ("launch requires --app");

        if (appName.isEmpty())
            throw std::runtime_error ("launch requires --app-name when it cannot be inferred from --app");

        if (sessionName.isEmpty())
            throw std::runtime_error ("launch requires --session when it cannot be inferred from --app-name");

        auto app = juce::File::createFileWithoutCheckingPath (appPath);
        auto artifactDir = juce::File::createFileWithoutCheckingPath (artifactPath);
        auto home = juce::File::createFileWithoutCheckingPath (homePath);
        auto sourceHome = juce::File::createFileWithoutCheckingPath (sourceHomePath);
        auto stdoutFile = juce::File::createFileWithoutCheckingPath (stdoutPath);
        auto stderrFile = juce::File::createFileWithoutCheckingPath (stderrPath);

        if (! artifactDir.createDirectory())
            throw std::runtime_error (("Could not create artifact directory " + artifactDir.getFullPathName()).toStdString());

        stdoutFile.getParentDirectory().createDirectory();
        stderrFile.getParentDirectory().createDirectory();

        juce::var profile;
        if (! noProfile)
            profile = prepareJuceProfile (home, sourceHome, appName, extraSettings, ! keepFilterState, ! keepAudioState);

        auto launchEnvironment = automationEnvironmentForLaunch (home, artifactDir, sessionName);
        juce::StringArray command;

#if JUCE_MAC
        auto bundle = macBundleForAppPath (app);
        if (bundle.isDirectory())
        {
            command.add ("/usr/bin/open");
            command.add ("-n");
            command.add ("-F");
            command.add ("--stdout");
            command.add (stdoutFile.getFullPathName());
            command.add ("--stderr");
            command.add (stderrFile.getFullPathName());

            for (const auto& assignment : launchEnvironment)
            {
                command.add ("--env");
                command.add (assignment);
            }

            command.add (bundle.getFullPathName());

            if (! appArgs.isEmpty())
            {
                command.add ("--args");
                command.addArray (appArgs);
            }
        }
        else
#endif
        {
#if JUCE_WINDOWS
            command.add (app.getFullPathName());
            command.addArray (appArgs);
#else
            command.add ("/usr/bin/env");
            addEnvironmentToCommand (command, launchEnvironment);
            command.add (app.getFullPathName());
            command.addArray (appArgs);
#endif
        }

#if JUCE_WINDOWS
        launchWindowsApp (app, appArgs, launchEnvironment, stdoutFile, stderrFile);
#else
        juce::ChildProcess process;
        if (! process.start (command, 0))
            throw std::runtime_error ("Could not launch app");

        process.waitForProcessToFinish (2000);
#endif
        auto session = noWait ? juce::var() : waitForSession (sessionName, timeoutMs);

        if (! noWait && session.getDynamicObject() == nullptr)
            throw std::runtime_error (("Timed out waiting for jucewright session '" + sessionName
                                       + "'. stdout=" + stdoutFile.getFullPathName()
                                       + " stderr=" + stderrFile.getFullPathName()).toStdString());

        juce::Array<juce::var> commandJson;
        for (const auto& arg : command)
            commandJson.add (arg);

        auto* result = new juce::DynamicObject();
        result->setProperty ("app", app.getFullPathName());
        result->setProperty ("appName", appName);
        result->setProperty ("session", sessionName);
        result->setProperty ("artifactDir", artifactDir.getFullPathName());
        result->setProperty ("home", home.getFullPathName());
        result->setProperty ("stdout", stdoutFile.getFullPathName());
        result->setProperty ("stderr", stderrFile.getFullPathName());
        result->setProperty ("command", juce::var (commandJson));

        juce::Array<juce::var> environmentJson;
        for (const auto& assignment : launchEnvironment)
            environmentJson.add (assignment);

        result->setProperty ("environment", juce::var (environmentJson));

        if (! profile.isVoid())
            result->setProperty ("profile", profile);

        if (session.getDynamicObject() != nullptr)
            result->setProperty ("matchedSession", session);

        return juce::var (result);
    }
