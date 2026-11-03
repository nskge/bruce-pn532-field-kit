#!/usr/bin/env bash
set -euo pipefail

action="${1:-list}"
host="${2:-127.0.0.1}"
port="${3:-45832}"
dump="${4:-}"
known_key="${5:-}"

missing=()
for command in socat nfc-list mfoc; do
    command -v "$command" >/dev/null 2>&1 || missing+=("$command")
done
if ((${#missing[@]})); then
    echo "Installing missing NFC tools in WSL..."
    sudo apt-get update
    sudo apt-get install -y socat libnfc-bin mfoc
fi

pty="/tmp/bruce-pn532-$$"
socat "PTY,raw,echo=0,link=$pty,mode=600" "TCP:$host:$port" 2>/dev/null &
socat_pid=$!
trap 'kill "$socat_pid" 2>/dev/null || true; rm -f "$pty"' EXIT
for _ in {1..50}; do
    [[ -e "$pty" ]] && break
    kill -0 "$socat_pid" 2>/dev/null || { echo 'TCP relay did not connect' >&2; exit 1; }
    sleep 0.1
done
[[ -e "$pty" ]] || { echo 'PTY was not created' >&2; exit 1; }
export LIBNFC_DEFAULT_DEVICE="pn532_uart:$pty"
export LIBNFC_DEVICE="pn532_uart:$pty"
export LIBNFC_AUTO_SCAN=false

case "$action" in
    list) nfc-list ;;
    recover)
        [[ -n "$dump" ]] || { echo 'Missing output dump path' >&2; exit 2; }
        if [[ -n "$known_key" ]]; then
            [[ "$known_key" =~ ^[0-9A-Fa-f]{12}$ ]] || { echo 'Known key must be 12 hex characters' >&2; exit 2; }
            mfoc -k "$known_key" -O "$dump"
        else
            mfoc -O "$dump"
        fi
        echo "Saved dump to $dump"
        ;;
    *) echo "Unknown action: $action" >&2; exit 2 ;;
esac
