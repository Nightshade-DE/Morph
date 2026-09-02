#!/usr/bin/env bash
# Local developer convenience wrapper.
# - Reconfigure a chosen Meson build dir.
# - Compile and run automated tests.
# - Run the nested smoke check as a quick runtime sanity pass.

set -euo pipefail

# Allow an alternative build dir while keeping default workflow simple.
build_dir="${1:-build}"

echo "[local-build-test] configure: ${build_dir}"
meson setup "${build_dir}" --reconfigure

echo "[local-build-test] compile: ${build_dir}"
meson compile -C "${build_dir}"

echo "[local-build-test] test: ${build_dir}"
meson test -C "${build_dir}" --print-errorlogs

echo "[local-build-test] nested smoke"
# Smoke test runs launcher/runtime integration after build + tests passed.
bash ./scripts/test-nested-smoke.sh "${build_dir}"

echo "[local-build-test] ok"
