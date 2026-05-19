#include <jucewright/jucewright.h>
#include "test_support.h"

#if JUCEWRIGHT_ENABLE_AUTOMATION
    #include <iostream>
    #include <stdexcept>
#endif

namespace
{
    using namespace jucewright_test;

#if JUCEWRIGHT_ENABLE_AUTOMATION
    #include "automation_fixture/support.ipp"
    #include "automation_fixture/self_test.ipp"
#endif
    #include "automation_fixture/ui_components.ipp"
}

#include "automation_fixture/app.ipp"
