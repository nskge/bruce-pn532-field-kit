"""Use a Bruce PN532 bridge with local libnfc tools (Linux/macOS/Windows).

Requires pyserial plus native nfc-list and mfoc executables. Windows users can
also use nfc-windows.ps1 for an automated Kali/WSL backend.
"""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

import serial
from serial.tools import list_ports


EXIT_SEQUENCE = b"\x1bBRUCE_EXIT\n"


def detect_port(explicit):
    if explicit:
        return explicit
    candidates = [
        p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001
    ]
    if len(candidates) != 1:
        raise RuntimeError("Specify --port; expected exactly one ESP32-S3 USB serial device")
    return candidates[0]


def send_command(port, command, expect_ready):
    with serial.Serial(port, 115200, timeout=0.1, write_timeout=5) as device:
        time.sleep(5 if expect_ready else 0.2)
        device.reset_input_buffer()
        device.write(command)
        if not expect_ready:
            return
        deadline = time.monotonic() + 15
        response = bytearray()
        while b"BRIDGE_READY" not in response and time.monotonic() < deadline:
            response.extend(device.read(512))
        if b"BRIDGE_READY" not in response:
            raise RuntimeError("PN532 bridge did not start; check firmware and selected port")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["list", "recover"])
    parser.add_argument("--port", help="USB serial port; auto-detects one ESP32-S3")
    parser.add_argument("--known-key", help="Known MIFARE Classic key, 12 hex digits")
    parser.add_argument("--output", type=Path, help="Output .mfd file for recover")
    args = parser.parse_args()
    if args.known_key and not re.fullmatch(r"[0-9a-fA-F]{12}", args.known_key):
        parser.error("--known-key must be exactly 12 hexadecimal digits")
    executable = "nfc-list" if args.action == "list" else "mfoc"
    if not shutil.which(executable):
        raise RuntimeError(f"{executable} is not installed; see docs/platforms.md")

    port = detect_port(args.port)
    output = args.output or Path.home() / "Downloads" / f"bruce-nfc-{time.strftime('%Y%m%d-%H%M%S')}.mfd"
    if args.action == "recover":
        output.parent.mkdir(parents=True, exist_ok=True)
    send_command(port, b"rfid bridge\n", expect_ready=True)
    env = os.environ.copy()
    env["LIBNFC_DEVICE"] = f"pn532_uart:{port}"
    env["LIBNFC_AUTO_SCAN"] = "false"
    command = [executable]
    if args.action == "recover":
        if args.known_key:
            command.extend(["-k", args.known_key])
        command.extend(["-O", str(output)])
    try:
        result = subprocess.run(command, env=env, check=False)
        if result.returncode:
            raise RuntimeError(f"{executable} exited with status {result.returncode}")
        if args.action == "recover":
            print(f"Dump saved: {output}")
    finally:
        try:
            send_command(port, EXIT_SEQUENCE, expect_ready=False)
        except serial.SerialException:
            print("Bridge exit could not be sent; press Back or reset the T-Embed")


if __name__ == "__main__":
    main()
