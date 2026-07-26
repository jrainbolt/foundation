#!/bin/sh
set -eu

expected_tag="$(tr -d '\r\n' < GODOT_CPP_VERSION)"
expected_commit="$(tr -d '\r\n' < GODOT_CPP_COMMIT)"
if [ -z "${GODOT_CPP_DIR:-}" ]; then
    echo "Set GODOT_CPP_DIR to a godot-cpp checkout at ${expected_tag}." >&2
    exit 1
fi
actual_tag="$(git -C "$GODOT_CPP_DIR" describe --tags --exact-match 2>/dev/null || true)"
actual_commit="$(git -C "$GODOT_CPP_DIR" rev-parse HEAD 2>/dev/null || true)"
if [ "$actual_tag" != "$expected_tag" ]; then
    echo "Expected godot-cpp tag ${expected_tag}; found ${actual_tag:-untagged}." >&2
    exit 1
fi
if [ "$actual_commit" != "$expected_commit" ]; then
    echo "Expected godot-cpp commit ${expected_commit}; found ${actual_commit:-unknown}." >&2
    exit 1
fi

exec scons platform="${1:-macos}" target="${2:-template_debug}"
