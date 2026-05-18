# Jucewright Development Guide

## Scope

Jucewright is a standalone JUCE module plus native CLI/MCP runner for
Playwright-style automation of JUCE apps and plugins. Keep it generic: features
should apply to arbitrary JUCE component trees, not to one host application.

## Origin Credit

Jucewright was extracted from automation work that began in
`melatonin_inspector`. Keep the README origin/credit section and MIT license
notice intact. Do not reintroduce old `melatonin_inspector`, `melatonin-ui`, or
`MELATONIN_*` compatibility aliases unless a maintainer explicitly asks for a
compatibility layer.

## Build and Test

```bash
cmake -S . -B Builds -DJUCEWRIGHT_BUILD_CLI=ON -DJUCEWRIGHT_BUILD_TESTS=ON
cmake --build Builds --target jucewright_cli automation_fixture --parallel 8
JUCEWRIGHT_BUILD_DIR="$PWD/Builds" Builds/automation_fixture_artefacts/automation_fixture
```

Useful variants:

```bash
cmake -S . -B Builds-juce7 -DJUCEWRIGHT_JUCE_GIT_TAG=7.0.12
cmake -S . -B Builds-demorunner -DJUCEWRIGHT_BUILD_DEMORUNNER_AUTOMATION=ON
```

Prefer testing latest JUCE `develop` first. JUCE 7 support is best effort; keep
changes conditional only when they are genuinely needed for older JUCE versions.

## Engineering Rules

- Use the existing JUCE/CMake style in nearby code.
- Keep public CLI, MCP, and environment names Jucewright-only.
- Prefer generic locator, actionability, launch, screenshot, and component-tree
  improvements over application-specific special cases.
- Keep app launch state isolation generic: use launcher/profile options rather
  than asking applications to add automation-only flags.
- Treat screenshot behavior as cross-platform infrastructure. Prefer component
  rendering plus explicit OpenGL/native-peer composition where possible, and only
  use platform-specific native screenshots behind clear source modes.
- Do not add broad fallback behavior that hides actionability bugs. Surface
  strict-mode ambiguity and missing locators clearly.
- Keep generated DemoRunner patching bounded to the top-level test target.
- Do not edit build artifacts under `Builds/`.

## Formatting

- Use C++17-compatible code.
- Match surrounding formatting; do not reformat unrelated code.
- Keep shell scripts POSIX/Bash-readable and fail clearly.
- Use concise comments only where the behavior is non-obvious.
