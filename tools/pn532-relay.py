"""Relay Bruce's PN532 UART-compatible serial mode to one local TCP client.

Requires: pip install pyserial
Example: python tools/pn532-relay.py --port COM7 --listen <windows-wsl-address>
"""

import argparse
import select
import socket
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Bruce serial port, e.g. COM7 or /dev/ttyACM0")
    parser.add_argument("--listen", default="127.0.0.1", help="TCP bind address")
    parser.add_argument("--tcp-port", type=int, default=45832)
    parser.add_argument("--write-mode", action="store_true", help="Start explicit bridge-write mode")
    args = parser.parse_args()

    with serial.Serial(args.port, 115200, timeout=0.1, write_timeout=5) as device:
        time.sleep(5)
        device.reset_input_buffer()
        command = b"rfid bridge-write\n" if args.write_mode else b"rfid bridge\n"
        device.write(command)
        deadline = time.monotonic() + 15
        startup = bytearray()
        while b"BRIDGE_READY" not in startup and time.monotonic() < deadline:
            startup.extend(device.read(512))
        if b"BRIDGE_READY" not in startup:
            raise RuntimeError("Bruce bridge did not start; reset device and check RFID module")
        device.reset_input_buffer()

        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
                server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                server.bind((args.listen, args.tcp_port))
                server.listen(1)
                print(f"Bridge ready on {args.listen}:{args.tcp_port}", flush=True)
                connection, _ = server.accept()
                with connection:
                    connection.setblocking(False)
                    while True:
                        readable, _, _ = select.select([connection], [], [], 0.01)
                        if readable:
                            payload = connection.recv(4096)
                            if not payload:
                                break
                            device.write(payload)
                        pending = device.in_waiting
                        if pending:
                            connection.sendall(device.read(pending))
        finally:
            device.write(b"\x1bBRUCE_EXIT\n")


if __name__ == "__main__":
    main()
