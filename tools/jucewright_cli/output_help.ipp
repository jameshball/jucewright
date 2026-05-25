    void printResult (const juce::var& result, bool preferJson = false)
    {
        if (!preferJson)
        {
            if (auto* object = result.getDynamicObject())
            {
                auto text = object->getProperty ("text").toString();

                if (text.isNotEmpty())
                {
                    std::cout << text.toStdString();
                    std::cout.flush();
                    return;
                }

                auto file = object->getProperty ("file").toString();

                if (file.isNotEmpty())
                {
                    std::cout << file.toStdString() << "\n";
                    std::cout.flush();
                    return;
                }
            }
        }

        std::cout << juce::JSON::toString (result, true).toStdString() << "\n";
        std::cout.flush();
    }

    void printHelp()
    {
        std::cout
            << "Usage:\n"
            << "  jucewright list\n"
            << "  jucewright mcp\n"
            << "  jucewright launch --app APP_OR_BUNDLE [--session NAME] [--artifact-dir DIR] [--home DIR] [--copy-setting FILE]\n"
            << "  jucewright prepare-juce-profile --home DIR --app-name NAME [--source-home DIR] [--copy-setting FILE] [--keep-filter-state] [--keep-audio-state]\n"
            << "  jucewright -s <session> capabilities\n"
            << "  jucewright -s <session> windows\n"
            << "  jucewright -s <session> trace-start --file trace.json\n"
            << "  jucewright -s <session> trace-stop\n"
            << "  jucewright -s <session> locator [--role role] [--name text] [--text text] [--selected] [--format json] [--timeout-ms n]\n"
            << "  jucewright -s <session> count [locator options] [--timeout-ms n]\n"
            << "  jucewright -s <session> describe <ref>|[locator options] [--depth n] [--full|--interesting|--minimal] [--timeout-ms n]\n"
            << "  jucewright -s <session> snapshot [--json|--format text|json] [--full|--interesting|--minimal] [--depth n] [--ref ref] [locator options] [--timeout-ms n]\n"
            << "  jucewright -s <session> screenshot [--target root|--ref m1-1] [--source auto|component|native] [--base64|--no-base64] [--clip-x n --clip-y n --clip-w n --clip-h n] [--timeout-ms n] [locator options] --file /tmp/root.png\n"
            << "  jucewright -s <session> click <ref>|[locator options] [--button left|right|middle] [--click-count n] [--position x,y]\n"
            << "  jucewright -s <session> dblclick <ref>|[locator options]\n"
            << "  jucewright -s <session> right-click <ref>|[locator options] [--menu-item name]\n"
            << "  jucewright -s <session> click-xy <x> <y> [--target root|window-1]\n"
            << "  jucewright -s <session> hover <x> <y> [--target root|window-1]\n"
            << "  jucewright -s <session> mouse-move <x> <y> [--target root|window-1]\n"
            << "  jucewright -s <session> mouse-down <x> <y> [--target root|window-1]\n"
            << "  jucewright -s <session> mouse-up <x> <y> [--target root|window-1]\n"
            << "  jucewright -s <session> wheel <x> <y> --dy amount [--target root|window-1]\n"
            << "  jucewright -s <session> drag-xy <x> <y> <toX> <toY> [--target root|window-1] [--steps n]\n"
            << "  jucewright -s <session> type <ref>|[locator options] <text>\n"
            << "  jucewright -s <session> fill <ref>|[locator options] <text>\n"
            << "  jucewright -s <session> clear <ref>|[locator options]\n"
            << "  jucewright -s <session> check <ref>|[locator options]\n"
            << "  jucewright -s <session> uncheck <ref>|[locator options]\n"
            << "  jucewright -s <session> set-checked <ref>|[locator options] true|false\n"
            << "  jucewright -s <session> set-value <ref>|[locator options] <value>\n"
            << "  jucewright -s <session> select-option <ref>|[locator options] --text name|--index n|--id n [--menu-item name]\n"
            << "  jucewright -s <session> select-tab <ref>|[locator options] --name tab|--index n\n"
            << "  jucewright -s <session> press <key> [--ref m1|locator options]\n"
            << "  jucewright -s <session> key-down <key> [--ref m1]\n"
            << "  jucewright -s <session> key-up <key> [--ref m1]\n"
            << "  jucewright -s <session> drag <ref>|[locator options] --dx n --dy n [--steps n] [--position x,y]\n"
            << "  jucewright -s <session> drag-to <ref>|[locator options] <target-ref>|[target locator options] [--steps n]\n"
            << "  jucewright -s <session> drop <ref>|[locator options] --description text [--position x,y]\n"
            << "  jucewright -s <session> drop-files <ref>|[locator options] --file path [--position x,y]\n"
            << "  jucewright -s <session> resize-window [target|--target root] --w n --h n\n"
            << "  jucewright -s <session> wait --ms n\n"
            << "  jucewright -s <session> wait-for-ref <ref> [--timeout-ms n]\n"
            << "  jucewright -s <session> wait-for-locator [locator options] [--timeout-ms n]\n"
            << "  jucewright -s <session> wait-for-text <text> [--timeout-ms n]\n"
            << "  jucewright -s <session> wait-for-value <ref>|[locator options] --value value [--timeout-ms n]\n"
            << "  jucewright -s <session> wait-for-snapshot-change --state-hash hash [--timeout-ms n]\n"
            << "\nSnapshot defaults to a compact interesting tree. Use --full for the complete component dump.\n"
            << "Screenshot base64 is off by default for CLI; use --base64 to include it. Use --clip-x/y/w/h to crop.\n";
    }

    juce::String popFront (juce::StringArray& args)
    {
        if (args.isEmpty())
            return {};

        auto value = args[0];
        args.remove (0);
        return value;
    }
