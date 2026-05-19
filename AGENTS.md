# Jucewright Notes

- Keep Jucewright generic to JUCE apps/plugins; avoid app-specific automation hooks.
- Public API, env vars, CLI, and MCP names should stay Jucewright-only.
- Start with README.md for setup and docs/automation-protocol.md for CLI/MCP behavior.
- Keep those docs updated when behavior changes.
- Keep the README origin credit and MIT license notice intact.
- Do not edit build artifacts under `Builds/`.
- For builds, tests, deployments, and exhaustive automation runs, prefer one blocking command with a long timeout and wait for completion. Avoid frequent short polling or progress checks unless the user asks for live updates or the command is genuinely interactive.

```bash
cmake -S . -B Builds -DJUCEWRIGHT_BUILD_CLI=ON -DJUCEWRIGHT_BUILD_TESTS=ON
cmake --build Builds --target jucewright_cli automation_fixture --parallel 8
JUCEWRIGHT_BUILD_DIR="$PWD/Builds" Builds/automation_fixture_artefacts/automation_fixture.app/Contents/MacOS/automation_fixture
```
