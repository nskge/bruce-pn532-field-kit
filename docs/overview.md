# Bruce PN532 Field Kit

An experimental Bruce firmware fork for the LilyGo T-Embed CC1101 and its integrated PN532 NFC controller. The name describes the actual hardware and workflow: Bruce is the user interface and ESP32 application, PN532 is the 13.56 MHz radio controller, and Field Kit is a portable set of reader and key-list tools. This is not Proxmark3 firmware.

## What is on the device

| Feature | Where it runs | What it does |
| --- | --- | --- |
| Standard Bruce RFID tools | T-Embed | Read, save, and use supported RFID/NFC tags according to Bruce and reader capabilities. |
| MIFARE common-key check | T-Embed | Try Bruce's 15 built-in MIFARE Classic keys while reading sectors. |
| MIFARE saved-list check | T-Embed | Try built-in keys plus valid 6-byte keys in `/BruceRFID/keys.conf`. |
| Wordlist import | T-Embed | Select a `.txt`, `.dic`, `.keys`, or `.conf` file on SD or LittleFS; merge its valid one-key-per-line entries into `keys.conf`. |
| Direct wordlist check | T-Embed | Select a list on SD or internal storage and test only that file plus the built-in keys, without changing the saved list. |
| PN532 USB bridge | T-Embed + host | Forward PN532 commands to libnfc. It allows host tools such as `mfoc` to use the built-in PN532. |

Under **RFID > MIFARE keys**, choose **Try common keys**, **Try saved list**, **Import internal**, or **Import from SD**. The last item appears when an SD card mounts. Place a wordlist in `/BruceRFID` for the file picker. Entries are exactly 12 hexadecimal characters (6 bytes), one per line. Blank lines and `#` or `//` comments are ignored. Imports are limited to 128 KiB and a total of 4096 saved keys. Imports merge with existing keys; they do not erase them. The normal **Read tag** option also uses both built-in and saved keys.

To try a short list without scanning the entire saved dictionary, choose **Try internal file** or **Try SD file** and select the file. These options do not import or modify saved keys. The file limit is 128 KiB and 4096 unique keys. The reader displays per-sector key-search progress; Back cancels the scan. A long dictionary still takes time for each authentication attempt.

The key checks are a **dictionary attack**: the device tests candidate keys against MIFARE Classic authentication. They do not enumerate all 2^48 possible keys and do not calculate unknown keys from card responses. A large list can take a long time because each candidate needs an RF exchange, and a stable card position matters. Press Back to cancel a long on-device read. MIFARE Classic uses separate keys A and B per sector and access bits determine what authenticated operations are permitted.

## What is still host-assisted

The USB bridge was validated with libnfc and `mfoc` on a host. `mfoc` can sometimes recover additional MIFARE Classic keys using a known key and a nested authentication weakness. The algorithm runs on the host in this version; the PN532 carries the RF exchanges. The bridge does not contain the `mfoc` algorithm, and the Bruce JavaScript RFID API exposes high-level read/write/key-add operations rather than a complete low-level nested-attack implementation.

For Windows users, `tools/nfc-windows.ps1` supports a tested Kali/WSL path and a native path using tools compiled from pinned official sources. Linux and macOS use `tools/nfc-unix.sh`. Run `List`/`list` to check the reader, then `Recover`/`recover` with an authorized card in the field. The generated dump stays in the host's Downloads folder; it is never part of this repository. See [platforms.md](platforms.md) for setup and validation status.

Porting nested recovery to the ESP32 is a separate development project involving PN532 raw exchanges, nonce handling, Crypto1 analysis, timing, cancellation, and card-specific failure cases. It may be possible for some cards, but this fork does not claim it. Some MIFARE Classic variants resist `mfoc`'s nested method. A full 48-bit exhaustive search is impractical on this device.

## Why Proxmark3 firmware is not a drop-in

Proxmark3 firmware targets its own microcontroller, FPGA, analog front end, and antennas. This T-Embed has an ESP32-S3 and a PN532 controller with a different command interface and RF access. Software features can be ported or reimplemented selectively, but flashing Proxmark3 firmware onto the T-Embed will not turn its PN532 into Proxmark3 hardware.

## Safety and release status

The two key-check entries start in read mode; Bruce's reader screen can switch to other existing modes after pressing OK. Importing a list updates Bruce's local key file. Host card writing requires the explicitly named `rfid bridge-write` serial mode. Make a full dump before modifying keys, access bits, or data on a card.

The code and documentation are English and contain no private card dump or recovered key. See [development status](development-status.md) for the exact checks completed on the development board. This is an experimental fork and not an official Bruce release.
