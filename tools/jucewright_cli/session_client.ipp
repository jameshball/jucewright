    juce::File sessionsDirectory()
    {
        const char* temp = std::getenv (
#if JUCE_WINDOWS
            "TEMP"
#else
            "TMPDIR"
#endif
        );

        return juce::File (temp != nullptr ? juce::String::fromUTF8 (temp) : juce::String ("/tmp"))
            .getChildFile ("jucewright")
            .getChildFile ("sessions");
    }

    bool canConnect (juce::DynamicObject& session)
    {
        juce::StreamingSocket socket;
        const auto host = session.getProperty ("host").toString();
        const auto port = (int) session.getProperty ("port");

        return host.isNotEmpty() && port > 0 && socket.connect (host, port, 250);
    }

    juce::Array<juce::var> loadSessions()
    {
        juce::Array<juce::var> sessions;
        for (const auto& entry : juce::RangedDirectoryIterator (sessionsDirectory(), false, "*.json", juce::File::findFiles))
        {
            auto file = entry.getFile();
            auto parsed = juce::JSON::parse (file.loadFileAsString());

            if (auto* session = parsed.getDynamicObject())
            {
                session->setProperty ("file", file.getFullPathName());
                session->setProperty ("modifiedAtMs", (double) file.getLastModificationTime().toMilliseconds());

                if (canConnect (*session))
                    sessions.add (parsed);
            }
        }

        return sessions;
    }

    juce::var findSession (const juce::String& requestedName)
    {
        auto sessions = loadSessions();

        if (requestedName.isEmpty() && sessions.size() == 1)
            return sessions.getFirst();

        if (requestedName.isEmpty())
            return {};

        juce::var bestMatch;
        double bestModifiedAt = -1.0;

        for (auto& session : sessions)
        {
            auto* object = session.getDynamicObject();

            if (object == nullptr)
                continue;

            if (object->getProperty ("session").toString() == requestedName
                || object->getProperty ("pid").toString() == requestedName
                || object->getProperty ("file").toString().contains (requestedName))
            {
                const auto modifiedAt = (double) object->getProperty ("modifiedAtMs");

                if (modifiedAt > bestModifiedAt)
                {
                    bestModifiedAt = modifiedAt;
                    bestMatch = session;
                }
            }
        }

        return bestMatch;
    }

    juce::String readLine (juce::StreamingSocket& socket, int timeoutMs)
    {
        std::string bytes;
        char buffer[1024] {};
        const auto deadline = juce::Time::currentTimeMillis() + juce::jmax (1, timeoutMs);

        while (juce::Time::currentTimeMillis() < deadline)
        {
            const auto remaining = (int) (deadline - juce::Time::currentTimeMillis());
            const auto ready = socket.waitUntilReady (true, juce::jlimit (1, 250, remaining));

            if (ready < 0)
                break;

            if (ready == 0)
                continue;

            const auto bytesRead = socket.read (buffer, (int) sizeof (buffer), false);

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

    int responseTimeoutMs (const juce::String& method, const juce::var& params)
    {
        int endpointTimeoutMs = method == "wait" ? 250 : (method.startsWith ("wait_for") ? 5000 : 5000);

        if (auto* object = params.getDynamicObject())
        {
            auto timeout = object->getProperty (method == "wait" ? "ms" : "timeoutMs");

            if (!timeout.isVoid())
                endpointTimeoutMs = (int) timeout;
        }

        return juce::jlimit (5000, 35000, endpointTimeoutMs + 2000);
    }

    juce::String endpointErrorMessage (juce::DynamicObject& responseObject)
    {
        auto* error = responseObject.getProperty ("error").getDynamicObject();
        auto message = error != nullptr ? error->getProperty ("message").toString() : "Unknown automation error";

        if (error != nullptr)
        {
            auto code = error->getProperty ("code").toString();

            if (code.isNotEmpty())
                message = code + ": " + message;

            auto matchCount = error->getProperty ("matchCount");

            if (!matchCount.isVoid())
                message << "\nmatchCount=" << matchCount.toString();

            auto matches = error->getProperty ("matches");

            if (!matches.isVoid())
                message << "\nmatches=" << juce::JSON::toString (matches, true);

            auto suggested = error->getProperty ("suggestedNextCommand").toString();

            if (suggested.isNotEmpty())
                message << "\nsuggestedNextCommand=" << suggested;
        }

        return message;
    }

    juce::var requestEnvelope (juce::DynamicObject& session, const juce::String& method, juce::var params)
    {
        juce::StreamingSocket socket;
        const auto host = session.getProperty ("host").toString();
        const auto port = (int) session.getProperty ("port");

        if (!socket.connect (host, port, 3000))
            throw std::runtime_error ("Could not connect to " + host.toStdString() + ":" + std::to_string (port));

        auto* requestObject = new juce::DynamicObject();
        requestObject->setProperty ("id", "1");
        requestObject->setProperty ("token", session.getProperty ("token"));
        requestObject->setProperty ("method", method);
        requestObject->setProperty ("params", params);

        auto payload = juce::JSON::toString (juce::var (requestObject), true) + "\n";
        socket.write (payload.toRawUTF8(), (int) payload.getNumBytesAsUTF8());

        auto response = juce::JSON::parse (readLine (socket, responseTimeoutMs (method, params)));
        auto* responseObject = response.getDynamicObject();

        if (responseObject == nullptr)
            throw std::runtime_error ("Timed out waiting for automation endpoint response");

        return response;
    }

    juce::var request (juce::DynamicObject& session, const juce::String& method, juce::var params)
    {
        auto response = requestEnvelope (session, method, params);
        auto* responseObject = response.getDynamicObject();

        if (responseObject == nullptr)
            throw std::runtime_error ("Invalid response from automation endpoint");

        if (! (bool) responseObject->getProperty ("ok"))
            throw std::runtime_error (endpointErrorMessage (*responseObject).toStdString());

        return responseObject->getProperty ("result");
    }
