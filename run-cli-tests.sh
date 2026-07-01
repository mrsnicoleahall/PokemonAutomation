#!/usr/bin/env bash
# Run PokemonAutomation command-line (unit) tests safely and reproducibly.
#
# Why this script exists:
#   - v0.30 shows a BLOCKING modal dialog if a SerialPrograms-Settings.json exists
#     in the current working directory alongside the one in ~/Library/Application
#     Support/SerialPrograms/UserSettings/. Never create a cwd settings file.
#   - The test runner is the MAIN app binary with --command-line-test-mode, NOT
#     SerialProgramsCommandLine (that's an unrelated serial tool).
#   - QT_QPA_PLATFORM=offscreen guarantees no window/dialog can ever appear on the
#     user's screen, even in an unexpected edge case.
#   - macOS has no `timeout`; we use a manual watchdog.
#
# Usage:
#   ./run-cli-tests.sh    # runs ALL tests under repo-root CommandLineTests/
#
# The app only supports --command-line-test-mode and --command-line-test-folder
# (no per-test flag), so this runs the whole CommandLineTests/ root. That root holds
# only this project's own dummy fixtures, so it is fast and scoped to our tests.
# Test fixtures live at repo-root CommandLineTests/<Space>/<Object>/<file>.png
# (committed, so tests are reproducible). TEST_MAP key = "<Space>_<Object>".

set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP="$REPO_ROOT/build_mac/SerialPrograms.app/Contents/MacOS/SerialPrograms"
TEST_ROOT="$REPO_ROOT/CommandLineTests"

if [[ ! -x "$APP" ]]; then
    echo "ERROR: app not built at $APP (run: cd build_mac && cmake --build . -j 10)" >&2
    exit 1
fi

# Guard: refuse to run if a stray cwd settings file would trigger the dual-file dialog.
if [[ -f "$REPO_ROOT/build_mac/SerialPrograms-Settings.json" ]]; then
    echo "ERROR: build_mac/SerialPrograms-Settings.json exists — delete it (it triggers the" >&2
    echo "       'two settings files' modal). This script uses flags, not a cwd settings file." >&2
    exit 1
fi

ARGS=(--command-line-test-mode --command-line-test-folder "$TEST_ROOT")

OUT="$(mktemp)"
( cd "$REPO_ROOT/build_mac" && QT_QPA_PLATFORM=offscreen "$APP" "${ARGS[@]}" >"$OUT" 2>&1 & pid=$!
  ( sleep 300 && kill "$pid" 2>/dev/null ) & wd=$!
  wait "$pid" 2>/dev/null; rc=$?; kill "$wd" 2>/dev/null; exit $rc ) && rc=0 || rc=$?

grep -iE 'test|pass|fail|error|Looking for|Testing' "$OUT" || cat "$OUT"
echo "---- exit code: $rc ----"
rm -f "$OUT"
exit $rc
