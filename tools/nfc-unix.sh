#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
venv="$root/.venv-nfc"

if ! command -v python3 >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then brew install python; else
        echo 'Python 3 is required; install it with your distribution package manager.' >&2
        exit 1
    fi
fi

if ! command -v nfc-list >/dev/null 2>&1 || ! command -v mfoc >/dev/null 2>&1; then
    if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update
        sudo apt-get install -y libnfc-bin mfoc python3-venv
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y libnfc mfoc python3
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -S --needed --noconfirm libnfc mfoc python
    elif command -v brew >/dev/null 2>&1; then
        brew install libnfc mfoc
    else
        echo 'Install libnfc and mfoc with your package manager, then retry.' >&2
        exit 1
    fi
fi

if [[ ! -x "$venv/bin/python" ]]; then
    if ! python3 -m venv "$venv"; then
        if command -v apt-get >/dev/null 2>&1; then
            sudo apt-get install -y python3-venv
            python3 -m venv "$venv"
        else
            echo 'Python venv support is required; install it with your package manager.' >&2
            exit 1
        fi
    fi
fi
if ! "$venv/bin/python" -c 'import serial' >/dev/null 2>&1; then
    "$venv/bin/python" -m pip install pyserial
fi
exec "$venv/bin/python" "$root/tools/nfc-native.py" "$@"
