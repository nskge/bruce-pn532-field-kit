# PN532 bridge for Bruce on T-Embed CC1101

This branch extends Bruce 1.16.1 for the LilyGo T-Embed CC1101 with its integrated I2C PN532. It is a development build, not an official Bruce release.

## What works

- `rfid read` continues past protected MIFARE Classic sectors and reports which sectors authenticated, without printing keys.
- `rfid bridge` forwards PN532 UART frames from Bruce's USB serial port to the integrated PN532. It blocks common cleartext card write/value commands.
- `rfid bridge-write` explicitly enables card writes for a trusted host program.
- The RFID menu has **PN532 USB bridge**. Press Back to leave this mode and return to Bruce. The menu mode starts the guarded `rfid bridge` path.
- **RFID > MIFARE keys** offers standalone common-key and saved-list reads plus wordlist import from SD or LittleFS. See [overview.md](overview.md).
- A host running libnfc can use the PN532 as `pn532_uart`, including `nfc-list`, `nfc-mfclassic`, and `mfoc`.

The bridge's read mode is a convenience guard, not a security boundary: PN532 raw frames can carry opaque or encrypted operations. Connect only a trusted host. The write mode is entered by an explicit serial command. From the RFID menu, press Back to exit the bridge. Host helpers also send the `ESC BRUCE_EXIT LF` control sequence when they close; reset the T-Embed if the connection is interrupted before cleanup.

## Standalone capability and limits

Bruce's on-device dictionary reader needs no computer. MIFARE Classic key recovery with `mfoc` runs on the host; the T-Embed supplies the RF reader through its PN532. The host can be Linux, macOS, Windows with WSL, or native Windows with the packaged tools, subject to the validation status in [platforms.md](platforms.md). This build does **not** implement nested or hardnested key recovery on the ESP32. It does not make the T-Embed equivalent to a Proxmark3. An Android/Linux host could in principle replace a PC if it can run libnfc and mfoc and connect to the relay; that combination has not been validated here.

MIFARE Classic always requires authentication. Its default key `FFFFFFFFFFFF` is public, but it is still a key. The manufacturer block of a standard card is read-only. Changing a card's keys or access bits can make its original application stop accepting it; save a complete dump and all keys first.

## Build

From this repository, build the target with `platformio run -e lilygo-t-embed-cc1101`. The FastLED version is pinned to 3.10.3 because the newer compatible release collided with a board macro during this build. The application image is `.pio/build/lilygo-t-embed-cc1101/firmware.bin`; the combined image is `Bruce-lilygo-t-embed-cc1101.bin`. On the development T-Embed running the matching Bruce partition layout, the application-only image was flashed at `0x10000` and its hash verified. A full combined-image flash starts at `0x0` and also replaces the bootloader and partition table. Keep a backup of the device flash before replacing firmware, and verify the board and partition layout before flashing either image.

## Host bridge

### Windows launcher

With the T-Embed connected over USB, open PowerShell in this repository:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\nfc-windows.ps1 -Action List
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\nfc-windows.ps1 -Action Recover
```

The launcher finds the ESP32-S3 USB serial port (or accepts `-ComPort COM7`). If a `dist/windows-host` package is present, it uses native Windows `libnfc` and `mfoc` without WSL. Otherwise it uses Kali/Ubuntu WSL, finds the Windows address seen by WSL, starts a temporary serial-to-TCP bridge, and installs missing `socat`, `libnfc-bin`, and `mfoc` packages using `sudo apt-get`. `Recover` saves a timestamped `.mfd` to Windows Downloads. See [platforms.md](platforms.md) for the source-built native package and other Linux distributions. Package installation may ask for your Linux password. The launcher does not change PowerShell's system execution policy.

The helper sends an exit sequence when the host closes so the T-Embed can return to Bruce without a reset. If a host process crashes or the USB link is interrupted, use the device's Back control or reset button. The launcher does not log raw NFC traffic or keys.

### Manual relay

Install Python 3 and `pyserial` on the USB host. Reset the T-Embed, then run:

```text
python tools/pn532-relay.py --port COM7 --listen <host-address-visible-from-Linux>
```

On a native Linux host, use its serial port and the default loopback address. With Kali under WSL, use the Windows WSL adapter address for `--listen` and create a PTY in Kali:

```sh
socat PTY,raw,echo=0,link=/tmp/ttyPN532,mode=600 TCP:<windows-wsl-address>:45832
LIBNFC_DEFAULT_DEVICE=pn532_uart:/tmp/ttyPN532 nfc-list
LIBNFC_DEFAULT_DEVICE=pn532_uart:/tmp/ttyPN532 mfoc -O card.mfd
```

Run `socat` in a separate terminal. If libnfc leaves the PTY claimed after an error, stop the relay and `socat`, then reconnect. Reset the T-Embed if it remains in bridge mode. `mfoc` needs at least one known key on the same card. It can fail on cards whose nonce generation resists its nested attack.

To enable host writes, start the relay with `--write-mode`. Only use that mode with a trusted program and a card you intend to modify.

## Privacy and release checklist

The repository must not include card dumps, UIDs, recovered keys, firmware backups, serial traces, or machine-specific IP addresses. The root `.gitignore` excludes the local audit artifacts created during development. Review `git status` and the staged diff before publishing. Do not paste recovered keys into issues or release notes.

## Proven on one device

The local development firmware was tested on a T-Embed CC1101 with integrated PN532. The bridge opened in libnfc 1.8.0, and `mfoc` 0.10.7 recovered keys on one MIFARE Classic 1K card. A complete 64-block read succeeded. This observation does not establish success on every MIFARE Classic card or board revision.
