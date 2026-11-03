# Development status

This experimental fork is based on Bruce 1.16.1 and targets the LilyGo T-Embed CC1101 with its integrated I2C PN532. The feature is implemented in the RFID menu as **PN532 USB bridge** and via the serial commands `rfid bridge` and `rfid bridge-write`.

## Verification on the development board

- The latest source built successfully with `platformio run -e lilygo-t-embed-cc1101` on November 3, 2026.
- The latest application image was flashed at `0x10000` on the development T-Embed; esptool verified its hash. The bootloader, partition table, and filesystem were not rewritten.
- A flashed development build returned the expected PN532 ACK and SAMConfiguration response `0000FF00FF000000FF02FED5151600`.
- The bridge's exit control sequence returned to the normal Bruce CLI; `info` then reported `Lilygo T-Embed CC1101` without a reset.
- The Windows + Kali/WSL launcher opened the reader with libnfc 1.8.0, and the device returned to Bruce afterward. Automatic COM detection and backend selection also worked.
- Windows native `libnfc` 1.8.0 and `mfoc` 0.10.7 were cross-compiled from pinned official commits. Native `nfc-list.exe` opened the reader directly on COM7, and `mfoc.exe -h` ran. The clean-room build script reproduced the package and SHA-256 manifest.
- After the latest flash, serial navigation opened **RFID > MIFARE keys** and showed the common-key, saved-list, saved-key-count, and import entries. The saved-key-count action responded. Native Windows `nfc-list.exe` still opened the PN532.
- On November 3, 2026, a local MIFARE Classic 1K card was read through Bruce in 64 pages after prioritizing built-in Key B candidates before the saved dictionary. All 48 data blocks and 16 access-bit fields matched two independent host reads. Native Windows `mfoc` recovered both keys for all 16 sectors; a strict `nfc-mfclassic` read then completed 64/64 blocks with those keys. Dumps and keys remain outside the repository.
- The **Try common keys** and **Try saved list** menu entries each traversed all 16 sectors. The SD wordlist picker imported a disposable file containing an already saved public key, reported zero additions and zero skips, and returned to the MIFARE menu. The disposable file was removed.
- A second temporary dictionary entry was already present, so importing it also reported zero additions and zero skips. A single sector trailer was changed under its documented Key B write condition, then Bruce read all 64 pages and authenticated that sector with a saved Key A. The original trailer was restored. A fresh host recovery produced a dump whose first 1,024 bytes matched the pretest backup exactly. No card data or keys are stored in this repository.
- Kali/WSL `mfoc` recovery also completed on the restored card; its 1,024-byte dump matched the pretest backup byte for byte. A local human-readable report was generated outside this repository.
- A later standalone menu test changed one test sector to a built-in common key and authenticated it with Key A. The reader reported all 16 sectors in about 19 seconds. The original trailer was verified and the temporary random test keys were reapplied. The wordlist importer reported one new key. A subsequent saved-list read with an approximately 32 KiB existing list did not complete within 600 seconds and reported only four sectors before the timeout; the cause remains unverified. This is not evidence of successful saved-list recovery with that list.
- Earlier development builds completed a 64-block MIFARE Classic 1K read and a host-side `mfoc` key recovery. The final build was not retested with a card because none was present at the reader.

Physical menu buttons and direct Linux/macOS operation remain untested. The Python TCP relay and Linux/macOS launcher pass syntax checking. The Windows/WSL path was tested on the development host; native Windows `nfc-list`, `mfoc`, and `nfc-mfclassic` were exercised with the current build. A direct custom libnfc tool sometimes leaves the bridge requiring a T-Embed reset before the next serial command; the packaged list/recovery launcher was not observed to require that reset.

## Next steps

1. Test the Linux and macOS direct-serial launcher on physical hosts and the physical menu buttons.
2. Improve bridge cleanup after custom host programs that close during a PN532 exchange.
3. Review the public fork's GitHub Actions build and validate the Debian workflow on a physical host. The `upstream` remote is fetch-only by local configuration.

This repository intentionally excludes card dumps, recovered keys, local transport scripts, and device flash backups. See [pn532-bridge.md](pn532-bridge.md) for usage and limitations.
