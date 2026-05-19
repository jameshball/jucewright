    constexpr auto sessionName = "automation_fixture";


    void cleanupSessionFiles()
    {
        auto directory = sessionsDirectory();

        if (!directory.isDirectory())
            return;

        for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.json", juce::File::findFiles))
        {
            auto file = entry.getFile();
            auto parsed = juce::JSON::parse (file.loadFileAsString());

            if (auto* object = parsed.getDynamicObject())
                if (object->getProperty ("session").toString() == sessionName)
                    file.deleteFile();
        }
    }

    juce::File inferBuildDirectory()
    {
        const auto overridePath = juce::SystemStats::getEnvironmentVariable ("JUCEWRIGHT_BUILD_DIR", {});

        if (overridePath.isNotEmpty())
            return juce::File (overridePath);

        auto executable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    #if JUCE_MAC
        return executable.getParentDirectory()
                         .getParentDirectory()
                         .getParentDirectory()
                         .getParentDirectory()
                         .getParentDirectory();
    #elif JUCE_WINDOWS
        return executable.getParentDirectory().getParentDirectory().getParentDirectory();
    #else
        return executable.getParentDirectory().getParentDirectory();
    #endif
    }

    juce::File findJucewright()
    {
        const auto overridePath = juce::SystemStats::getEnvironmentVariable ("JUCEWRIGHT_CLI", {});

        if (overridePath.isNotEmpty())
            return juce::File (overridePath);

        auto buildDir = inferBuildDirectory();
        auto artefacts = buildDir.getChildFile ("jucewright_cli_artefacts");

    #if JUCE_WINDOWS
        const auto executableName = "jucewright.exe";

        for (auto configuration : { "Debug", "Release", "RelWithDebInfo", "MinSizeRel" })
        {
            auto candidate = artefacts.getChildFile (configuration).getChildFile (executableName);

            if (candidate.existsAsFile())
                return candidate;
        }

        return artefacts.getChildFile (executableName);
    #else
        auto targetNamedArtefacts = buildDir.getChildFile ("jucewright_artefacts").getChildFile ("jucewright");
        if (targetNamedArtefacts.existsAsFile())
            return targetNamedArtefacts;

        return artefacts.getChildFile ("jucewright");
    #endif
    }


    juce::var findNode (const juce::var& node, const std::function<bool (juce::DynamicObject&)>& predicate)
    {
        auto* object = node.getDynamicObject();

        if (object == nullptr)
            return {};

        if (predicate (*object))
            return node;

        auto children = object->getProperty ("children");

        if (children.isArray())
            for (const auto& child : *children.getArray())
                if (auto found = findNode (child, predicate); !found.isVoid())
                    return found;

        return {};
    }

    juce::var findByComponentName (const juce::var& snapshot, const juce::String& componentName)
    {
        auto tree = asObject (snapshot, "snapshot").getProperty ("tree");
        return findNode (tree, [&componentName] (juce::DynamicObject& node) {
            return node.getProperty ("componentName").toString() == componentName;
        });
    }

    juce::String refByComponentName (const juce::var& snapshot, const juce::String& componentName)
    {
        auto node = findByComponentName (snapshot, componentName);
        require (!node.isVoid(), "Could not find componentName " + componentName);
        return asObject (node, componentName).getProperty ("ref").toString();
    }

    juce::Rectangle<int> boundsOf (const juce::var& node)
    {
        auto bounds = asObject (node, "node").getProperty ("bounds");
        auto& boundsObject = asObject (bounds, "bounds");

        return { (int) boundsObject.getProperty ("x"),
                 (int) boundsObject.getProperty ("y"),
                 (int) boundsObject.getProperty ("w"),
                 (int) boundsObject.getProperty ("h") };
    }

    double valueOf (const juce::var& node)
    {
        return asObject (node, "node").getProperty ("value").toString().getDoubleValue();
    }

    void assertStatus (const juce::var& snapshot, const juce::String& expected)
    {
        auto status = findByComponentName (snapshot, "fixture.status");
        require (!status.isVoid(), "snapshot is missing fixture.status");

        auto& object = asObject (status, "fixture.status");
        auto actual = object.getProperty ("name").toString()
                      + "\n" + object.getProperty ("title").toString()
                      + "\n" + object.getProperty ("value").toString();

        require (actual.contains (expected), "expected status \"" + expected + "\", got \"" + actual + "\"");
    }

    int readBigEndianInt (const juce::MemoryBlock& bytes, size_t offset)
    {
        auto* data = static_cast<const unsigned char*> (bytes.getData());
        return ((int) data[offset] << 24) | ((int) data[offset + 1] << 16) | ((int) data[offset + 2] << 8) | (int) data[offset + 3];
    }

    juce::MemoryBlock loadPng (const juce::File& file, const juce::String& label)
    {
        juce::MemoryBlock bytes;
        require (file.loadFileAsData (bytes), label + " could not be read: " + file.getFullPathName());

        static constexpr unsigned char pngSignature[] { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
        require (bytes.getSize() > sizeof (pngSignature),
                 label + " is unexpectedly small: " + juce::String ((int) bytes.getSize()) + " bytes");
        require (bytes.getSize() >= sizeof (pngSignature), label + " is not a PNG: " + file.getFullPathName());

        auto* data = static_cast<const unsigned char*> (bytes.getData());

        for (size_t i = 0; i < sizeof (pngSignature); ++i)
            require (data[i] == pngSignature[i], label + " is not a PNG: " + file.getFullPathName());

        return bytes;
    }

    void assertPng (const juce::File& file, const juce::String& label)
    {
        loadPng (file, label);
    }

    void assertPngSize (const juce::File& file, int width, int height, const juce::String& label)
    {
        auto bytes = loadPng (file, label);
        auto actualWidth = readBigEndianInt (bytes, 16);
        auto actualHeight = readBigEndianInt (bytes, 20);

        require (actualWidth == width && actualHeight == height,
                 label + " expected " + juce::String (width) + "x" + juce::String (height)
                     + ", got " + juce::String (actualWidth) + "x" + juce::String (actualHeight));
    }

