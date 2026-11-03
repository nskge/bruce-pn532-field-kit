# Architecture and protocol notes

```text
MIFARE Classic card <~~13.56 MHz~~> PN532 <---I2C---> ESP32-S3 / Bruce
                                                 |
                                       USB CDC serial / PN532 frames
                                                 |
                           native libnfc, or TCP relay + libnfc in WSL
                                                 |
                                   nfc-list / nfc-mfclassic / mfoc
```

The T-Embed CC1101 contains an ESP32-S3 and an integrated PN532. Bruce talks to the PN532 over I2C. `rfid bridge` accepts standard PN532 UART frames on Bruce's USB serial port and forwards their commands to the PN532 over I2C; it returns the PN532 ACK and response. This is a transport adapter, so the host's `pn532_uart` driver can use a chip that is not physically wired to UART. The current bridge accepts normal PN532 frames up to 240 payload bytes and returns responses up to 64 frame bytes; larger or extended frames are outside its tested scope.

On Linux/macOS, libnfc can open the USB serial port directly. Native Windows libnfc can open the COM port directly with source-built tools. WSL cannot normally open the Windows COM port directly, so the Windows launcher keeps the serial port open and forwards bytes over a local TCP connection to `socat`, which exposes a pseudo-terminal to libnfc inside WSL. The host helpers send an escape control sequence to leave bridge mode after the tool exits.

## What the two key workflows do

**On-device dictionary:** Bruce has 15 common candidate keys and a local `/BruceRFID/keys.conf`. The new menu can import a one-key-per-line list and start a read using common keys alone or common plus saved keys. For each MIFARE Classic sector, the PN532 tries the built-in Key A and Key B candidates first, then the saved list for both key types if needed. A successful key permits reading only what the card's access bits allow. This process tests guesses; it does not infer a key outside the list. A full 48-bit exhaustive search is not practical here.

**Host nested recovery:** `mfoc` uses a known MIFARE Classic key and a weakness in some cards' authentication nonce behavior to derive other keys. It needs repeated low-level NFC exchanges, Crypto1 state recovery, and a host-side algorithm. The T-Embed carries those exchanges through the bridge. `mfoc` may fail on cards that do not exhibit the relevant weakness, even when the bridge works correctly. A normal key-list read and an `mfoc` recovery are different operations.

## Why this is not Proxmark3 firmware

The PN532 is a command-oriented NFC controller; it performs many RF tasks internally and exposes PN532 commands. Proxmark3 uses its own analog front end, FPGA, microcontroller, and LF/HF antennas, giving its firmware different low-level timing and signal access. Some host algorithms can be adapted to the PN532 route, as `mfoc` demonstrates, but this hardware cannot run Proxmark3 firmware or reproduce every Proxmark3 operation by software alone.

## Code map

| Component | Responsibility |
| --- | --- |
| `src/core/serial_commands/rfid_commands.cpp` | PN532 frame parser, I2C command forwarding, USB bridge commands, and exit control. |
| `src/core/menu_items/RFIDMenu.cpp` | RFID menu entries for key-list reads/import and bridge mode. |
| `src/core/mifare_keys_manager.cpp` | Validate, merge, and store MIFARE Classic candidate keys. |
| `src/modules/rfid/PN532.cpp` | PN532 reader, MIFARE Classic authentication and partial-sector read behavior. |
| `tools/nfc-windows.ps1` | Windows native or WSL host launcher. |
| `tools/nfc-native.py` and `tools/nfc-unix.sh` | Direct serial host workflow for Linux/macOS. |
| `tools/build-windows-host.sh` | Reproducible cross-build of official libnfc and mfoc sources for native Windows. |

## Primary references

- [NXP PN532 user manual](https://www.nxp.com/docs/en/user-guide/141520.pdf): PN532 commands and MIFARE reader operation.
- [NXP MIFARE Classic 1K data sheet](https://www.nxp.com/docs/en/data-sheet/MF1S50YYX_V1.pdf): sector memory, keys, access conditions, and manufacturer block.
- [libnfc](https://github.com/nfc-tools/libnfc): host driver and connection strings.
- [mfoc](https://github.com/nfc-tools/mfoc): nested recovery tool and known-key requirement.
- [Proxmark3 FPGA/ARM architecture notes](https://github.com/RfidResearchGroup/proxmark3/blob/master/doc/fpga_arm_notes.md): hardware model underlying Proxmark3 firmware.
