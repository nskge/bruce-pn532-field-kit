# Debian quick start

This guide uses a LilyGo T-Embed CC1101 already running this fork, its integrated PN532, and a MIFARE Classic card you are authorized to test. Keep a complete private backup before changing a card's keys or access bits. No card key or dump is in this repository.

The T-Embed is the **NFC reader**; USB connects it to Debian. The **PN532 bridge** forwards commands from Debian's `libnfc` to the reader. `nfc-list` detects the card, and `mfoc` runs on Debian to attempt nested MIFARE Classic key recovery. The launcher starts and stops bridge mode automatically. You do not need to select **PN532 USB bridge** on the device before running these commands. On-device key-list checks under **RFID > MIFARE keys** work without Debian.

## Install and connect

```sh
sudo apt update
sudo apt install -y git python3 python3-venv
git clone https://github.com/nskge/bruce-pn532-field-kit.git
cd bruce-pn532-field-kit
./tools/nfc-unix.sh list
```

The launcher installs missing `libnfc-bin`, `mfoc`, and `pyserial` on Debian. It creates a Python environment inside this checkout. Connect the T-Embed by USB and place the card against its PN532 antenna before running `list`. `list` checks the reader and card only; it does not recover keys or save a dump. The T-Embed may restart when the serial port opens; wait for the reader result.

If the script cannot open the USB serial port, identify it with `ls -l /dev/serial/by-id/` or `ls -l /dev/ttyACM*`. Pass `--port /dev/ttyACM0` if automatic detection finds more than one matching ESP32-S3. Debian often grants serial access through the `dialout` group; if your account lacks it, add the account with `sudo usermod -aG dialout "$USER"`, then sign out and back in. Check the group with `id -nG` before retrying.

## Recover and inspect

```sh
./tools/nfc-unix.sh recover
```

`recover` runs `mfoc` on Debian through the bridge. `mfoc` needs at least one known sector key and a card susceptible to its nested method; it tries common defaults unless you supply a known key with `--known-key` followed by exactly 12 hexadecimal digits. A successful `list` does not guarantee recovery. On success, the launcher writes a timestamped `.mfd` file to `~/Downloads`. Keep the dump private and do not commit it or paste keys into an issue. Record its filename, byte count, and SHA-256 locally:

```sh
ls -lh ~/Downloads/bruce-nfc-*.mfd
sha256sum ~/Downloads/bruce-nfc-*.mfd
```

Report whether `list` detected a card and whether `recover` completed, along with the output file size. Mask the UID and keys when sharing terminal output. If you temporarily change a card key for a test, restore it from the private backup before using the card in its original application.

## Troubleshooting

- If `list` says the serial port is busy, close other serial monitors and try again.
- If `list` opens the reader but finds no card, center the card and retry.
- If the bridge remains active after a host tool exits unexpectedly, press Back on the T-Embed or reset it, then retry.
- If recovery fails after the reader opens, keep the dump and terminal log private. Card firmware and nonce behavior can limit `mfoc` even when the PN532 bridge is working.

See [platforms.md](platforms.md) for Windows, WSL, other Linux distributions, and macOS.
