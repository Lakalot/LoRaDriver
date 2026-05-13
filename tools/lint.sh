#!/usr/bin/env bash
# Runs clang-format --dry-run --Werror on all source files. Non-zero exit if
# any file needs reformatting.
set -euo pipefail

cd "$(dirname "$0")/.."

mapfile -t files < <(git ls-files \
    'src/*.cpp' 'src/**/*.cpp' 'src/**/*.hpp' \
    'include/loradriver/*.hpp' 'include/loradriver/**/*.hpp' \
    'tests/host/*.cpp' 'tests/host/*.hpp')

if [[ ${#files[@]} -eq 0 ]]; then
    echo "No files to lint."
    exit 0
fi

clang-format --dry-run --Werror "${files[@]}"
