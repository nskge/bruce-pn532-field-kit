# Test the NFC fork from a clean Debian computer

This walkthrough covers the new MIFARE Classic menu and the USB host workflow. Use a LilyGo T-Embed CC1101 **already running this fork**, its integrated PN532, a USB data cable, and a MIFARE Classic card you own or are authorized to test. The official Bruce Web Flasher does not install this fork; for a new device, first read the [firmware build and flash notes](pn532-bridge.md#build). Keep card dumps and keys out of public repositories.

## 1. Check the standalone features on the T-Embed

These checks do not need a computer. Hold the card still over the PN532 antenna.

1. Open **RFID > MIFARE keys > Saved key count**. Note the count. Zero is fine.
2. Select **Try common keys**. The reader tests 15 built-in candidates against each MIFARE Classic sector and reads what authenticates. Note which sectors are accessible.
3. Select **Try saved list**. This adds keys from the device's `/BruceRFID/keys.conf` to the built-in candidates. If the saved list is empty, it should behave like the common-key check.
4. To exercise import, place a text file in `/BruceRFID` on an SD card or internal storage. Give it one **12-character hexadecimal key per line**. For a harmless example using a public default key, put `FFFFFFFFFFFF` on one line. Select **Import from SD** or **Import internal**, choose the file, then check **Saved key count** and run **Try saved list** again. Import merges and deduplicates entries, so a key already saved may add zero entries. The file limit is 128 KiB and the saved-list limit is 4096 keys.

The menu reads; it does not remove authentication. A sector protected by a key outside both lists can remain unreadable. To verify that a *new* saved candidate unlocks a sector, use a separate expendable test card whose key and backup you already control. Do not change the keys on an application card solely to exercise this menu.

## 2. Prepare Debian

Connect the T-Embed to the Debian computer by USB. Place the card on the PN532 and leave it in position. In a terminal:

```sh
sudo apt update
sudo apt install -y git python3 python3-venv
git clone https://github.com/nskge/bruce-pn532-field-kit.git
cd bruce-pn532-field-kit
./tools/nfc-unix.sh list
```

On first use, the launcher installs missing `libnfc-bin`, `mfoc`, and Python `pyserial`. It automatically finds a single ESP32-S3 serial device, starts Bruce's guarded PN532 bridge, runs `nfc-list`, and exits bridge mode. **Do not open the bridge menu first.** A successful `list` shows a reader and card; it does not prove any key was recovered.

If multiple ESP32-S3 serial ports are present, find the device with `ls -l /dev/serial/by-id/` and repeat the last command with `--port /dev/ttyACM0`, replacing the path as needed. If permission to open the port is denied, run `sudo usermod -aG dialout "$USER"`, then sign out and back in before retrying. Run the launcher as your normal user.

## 3. Try host-assisted recovery

With the card still centered:

```sh
./tools/nfc-unix.sh recover
```

This runs `mfoc` **on Debian** through the T-Embed's PN532. It needs a known key on the card and a card whose nonce behavior permits its nested method. `mfoc` tests common defaults; if you know a different sector key, the launcher accepts `--known-key` followed by its 12 hexadecimal digits. A working reader does not guarantee recovery.

On success, the launcher saves a timestamped `.mfd` in `~/Downloads`. For a MIFARE Classic 1K full dump, the file is 1024 bytes. Check locally:

```sh
ls -lh ~/Downloads/bruce-nfc-*.mfd
sha256sum ~/Downloads/bruce-nfc-*.mfd
```

Keep the dump and any recovered keys private. When asking for help, share the command outcome and file size with the UID and keys masked. If recovery fails, record the error; do not infer that the bridge is broken if `list` worked.

## 4. Check the bridge and finish

The `list` and `recover` commands each enter and leave bridge mode automatically. After either command finishes, the T-Embed should respond to its normal Bruce menu. The **RFID > PN532 USB bridge** menu item is for using a host tool manually; press Back to exit it. If a host process crashes mid-command and the device stays in bridge mode, press Back or reset the T-Embed.

The automatic workflow is read-oriented. Host card writing requires the separate `rfid bridge-write` serial command and a trusted tool; this guide does not change card memory. Restore any temporary test key from your private backup before using the card in its original application.

For Windows, WSL, other Linux distributions, and macOS, see [host setup](platforms.md). For architecture and limits, see the [project overview](overview.md).
