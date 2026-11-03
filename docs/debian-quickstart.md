# Debian quick start

This guide uses a LilyGo T-Embed CC1101 already running this fork, its integrated PN532, and a MIFARE Classic card you are authorized to test. Keep a complete private backup before changing a card's keys or access bits. No card key or dump is in this repository.

## Install and connect

```sh
sudo apt update
sudo apt install -y git python3 python3-venv
git clone https://github.com/nskge/bruce-pn532-field-kit.git
cd bruce-pn532-field-kit
./tools/nfc-unix.sh list
```

The launcher installs missing `libnfc-bin`, `mfoc`, and `pyserial` on Debian. It creates a Python environment inside this checkout. Connect the T-Embed by USB and place the card against its PN532 antenna before running `list`. The T-Embed may restart when the serial port opens; wait for the reader result.

If the script cannot open the USB serial port, identify it with `ls -l /dev/serial/by-id/` or `ls -l /dev/ttyACM*`. Pass `--port /dev/ttyACM0` if automatic detection finds more than one matching ESP32-S3. Debian often grants serial access through the `dialout` group; if your account lacks it, add the account with `sudo usermod -aG dialout "$USER"`, then sign out and back in. Check the group with `id -nG` before retrying.

## Recover and inspect

```sh
./tools/nfc-unix.sh recover
```

If you know a sector key, you can pass it as `--known-key` followed by exactly 12 hexadecimal digits. `mfoc` may recover keys on susceptible cards; a successful `list` does not guarantee recovery. The launcher writes a timestamped `.mfd` file to `~/Downloads`. Keep the dump private and do not commit it or paste keys into an issue. Record its filename, byte count, and SHA-256 locally:

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
