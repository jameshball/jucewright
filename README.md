# Jucewright

Playwright-style automation for JUCE apps and plugins.

Jucewright exposes an opt-in local automation endpoint for a `juce::Component`
tree, plus a native CLI/MCP server that can snapshot, locate, screenshot, and
interact with JUCE controls. It is designed for local development, E2E tests,
and LLM agents that need to browse native JUCE UIs without human involvement.

## Usage

Add the module after JUCE:

```cmake
add_subdirectory(path/to/jucewright)
target_link_libraries(YourTarget PRIVATE jucewright)
target_compile_definitions(YourTarget PRIVATE JUCEWRIGHT_ENABLE_AUTOMATION=1)
```

Enable the endpoint on your editor or root component:

```cpp
#include <jucewright/jucewright.h>

class Editor : public juce::Component
{
public:
    Editor()
    {
        automation.enableFromEnvironment (*this, "my-plugin");
    }

private:
    jucewright::Automation automation;
};
```

The endpoint only starts when `JUCEWRIGHT_AUTOMATION=1` is present. `JUCEWRIGHT_SESSION`
sets the public session name, and `JUCEWRIGHT_ARTIFACT_ROOT` constrains file outputs.

## CLI

Build the native CLI from the top-level CMake project:

```sh
cmake -S . -B Builds -DJUCEWRIGHT_BUILD_CLI=ON
cmake --build Builds --target jucewright_cli --parallel 4
```

The standalone build fetches JUCE `develop` by default. To test against a
specific JUCE release, pass a tag, branch, or commit:

```sh
cmake -S . -B Builds-juce7 -DJUCEWRIGHT_JUCE_GIT_TAG=7.0.12
cmake --build Builds-juce7 --parallel 4
```

Examples:

```sh
jucewright list
jucewright -s my-plugin snapshot
jucewright -s my-plugin locator --role button --name Apply --format json
jucewright -s my-plugin click --role button --name Apply
jucewright -s my-plugin screenshot --target root --file root.png
jucewright mcp
```

The CLI also includes a generic launcher for standalone JUCE apps:

```sh
jucewright launch --app /path/to/App.app --app-name App --session app
```

On macOS it launches `.app` bundles through `open` with `HOME` and
`CFFIXED_USER_HOME`; on Linux it sets `HOME` and XDG paths; on Windows it uses
`CreateProcessW` with an explicit environment block.

## Tests

```sh
cmake -S . -B Builds -DJUCEWRIGHT_BUILD_TESTS=ON
cmake --build Builds --target automation_fixture --parallel 4
JUCEWRIGHT_BUILD_DIR="$PWD/Builds" Builds/automation_fixture_artefacts/automation_fixture
```

See [docs/automation-protocol.md](docs/automation-protocol.md) for the full
protocol and command reference.

## Origin

Jucewright was extracted from automation work originally developed inside
[`melatonin_inspector`](https://github.com/sudara/melatonin_inspector). The
module keeps the automation layer independent from the visual inspector while
retaining MIT licensing.
