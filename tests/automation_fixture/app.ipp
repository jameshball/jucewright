class AutomationFixtureApp : public juce::JUCEApplication
{
public:
    void initialise (const juce::String&) override
    {
#if JUCEWRIGHT_ENABLE_AUTOMATION
        cleanupSessionFiles();
#endif

        mainWindow = std::make_unique<MainWindow> (getApplicationName());
        automation = std::make_unique<jucewright::Automation>();

#if JUCEWRIGHT_ENABLE_AUTOMATION
        jucewright::AutomationOptions options;
        options.sessionName = sessionName;
        options.allowFileWrite = true;
        options.artifactRoot = juce::SystemStats::getEnvironmentVariable ("JUCEWRIGHT_SCREENSHOT_DIR", {}).isNotEmpty()
                                   ? juce::File (juce::SystemStats::getEnvironmentVariable ("JUCEWRIGHT_SCREENSHOT_DIR", {}))
                                   : tempDirectory();
        automation->enable (mainWindow->content, options);
#endif

        juce::Process::makeForegroundProcess();
        mainWindow->toFront (true);

#if JUCEWRIGHT_ENABLE_AUTOMATION
        selfTest = std::make_unique<AutomationFixtureSelfTest> ([this] (int returnCode) {
            setApplicationReturnValue (returnCode);
            quit();
        });
        selfTest->startThread();
#endif
    }

    void shutdown() override
    {
#if JUCEWRIGHT_ENABLE_AUTOMATION
        selfTest = nullptr;
#endif
        automation = nullptr;
        mainWindow = nullptr;
    }

    const juce::String getApplicationName() override { return "automation_fixture"; }
    const juce::String getApplicationVersion() override { return "v1"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (name, juce::Colours::lightgrey, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentNonOwned (&content, true);
            centreWithSize (640, 420);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        AutomationRoot content;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<jucewright::Automation> automation;
#if JUCEWRIGHT_ENABLE_AUTOMATION
    std::unique_ptr<AutomationFixtureSelfTest> selfTest;
#endif
};

START_JUCE_APPLICATION (AutomationFixtureApp)
