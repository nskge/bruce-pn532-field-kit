# Host platforms

The firmware exposes the T-Embed's integrated PN532 through its USB serial port. The host runs `libnfc` and, for MIFARE Classic nested recovery, `mfoc`. The device's standalone dictionary reader does not need a host.

| Host | Launcher | Dependency setup | Validation |
| --- | --- | --- | --- |
| Windows + Kali/Ubuntu WSL | `tools/nfc-windows.ps1 -Backend WSL` | Installs missing `socat`, `libnfc-bin`, and `mfoc` using `apt-get` in WSL. WSL itself must already be installed. | Validated on Windows with Kali/WSL and a T-Embed. |
| Windows native | `tools/nfc-windows.ps1 -Backend Native` | Uses the host-tools package built from pinned official sources, or compatible tools in PATH. | `nfc-list.exe`, `nfc-mfclassic.exe`, and `mfoc.exe` were exercised with a local card. |
| Debian, Ubuntu, Kali, Fedora, Arch Linux | `tools/nfc-unix.sh` | Installs missing tools using apt, dnf, or pacman; creates a local Python environment for pyserial. | Script syntax checked. Direct USB operation remains to be tested on each distribution. |
| macOS | `tools/nfc-unix.sh` | Uses Homebrew for `libnfc`/`mfoc` and a local Python environment. | Script syntax checked; hardware path not yet tested. |

## Commands

Windows (PowerShell):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\nfc-windows.ps1 -Action List
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\nfc-windows.ps1 -Action Recover
```

`-Backend Auto` (the default) uses `dist/windows-host` or compatible native Windows tools if both executables are available; otherwise it uses WSL. Use `-ComPort COM7` if more than one matching ESP32-S3 serial device is connected. A Kali or Ubuntu WSL distribution can be selected with `-Distro <name>`.

Linux or macOS:

```sh
./tools/nfc-unix.sh list
./tools/nfc-unix.sh recover
```

Pass `--port /dev/ttyACM0` if automatic USB port detection is ambiguous. On Linux, the user must have permission to open the serial device. `recover` writes a timestamped `.mfd` to the host's Downloads folder. Keep that file private.

## Native Windows tools

The official [libnfc Windows build guide](https://github.com/nfc-tools/libnfc/blob/master/README-Windows.md) documents the platform; [mfoc](https://github.com/nfc-tools/mfoc) provides source but no official Windows binary release. This repository includes `tools/build-windows-host.sh`, which cross-compiles both from pinned official commits, applies two small compatibility patches, and writes `dist/windows-host` with a SHA-256 manifest and both licenses. It was run successfully under Kali/WSL and its tools were tested against the T-Embed on Windows. The GitHub Actions workflow is configured to build the same artifact on request or when the host build files change; check its run status before relying on an artifact.

On a Linux or WSL machine with MinGW, CMake, Autotools, and pkg-config installed, run `bash tools/build-windows-host.sh`. On Debian/Kali, those build dependencies can be installed with `sudo apt-get install mingw-w64 cmake autoconf automake libtool pkg-config`. On a Windows PC without WSL, extract the resulting package into this checkout's `dist/windows-host` folder so that `dist/windows-host/nfc-list.exe` and `dist/windows-host/mfoc.exe` exist. The PowerShell launcher will find that folder automatically, or accept `-ToolsDir <folder>`. No WSL or Python is required on that Windows PC.

This project does not silently download an unaudited third-party executable. Windows users who already have Kali/WSL can let the launcher install missing Linux tools automatically. When a published GitHub build artifact is available, Windows users without WSL can download the source-built package.

`mfoc` needs at least one known key and only works on susceptible MIFARE Classic cards. It is not a general password search for every NFC tag. This fork does not include `mfoc` source or binary in the firmware.
