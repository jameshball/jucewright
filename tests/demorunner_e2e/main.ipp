int main()
{
    try
    {
        DemoRunnerE2E().run();
        std::cout << "ok - DemoRunner automation e2e passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "DemoRunner automation e2e failed: " << e.what() << "\n";
        return 1;
    }
}
