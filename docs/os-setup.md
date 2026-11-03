# Use the NFC fork on common desktop systems

These steps use a LilyGo T-Embed CC1101 **already running this fork**, its built-in PN532, a USB data cable, and a MIFARE Classic card you are authorized to test. The official Bruce Web Flasher installs upstream Bruce, not this fork. See [firmware build and flash notes](pn532-bridge.md#build) for a new T-Embed.

**No computer:** On the device, open **RFID > MIFARE keys** to test common keys, import a list, and read sectors accessible with those keys. This works the same regardless of your desktop operating system.

**With a computer:** The launcher starts the PN532 USB bridge, runs `nfc-list` or `mfoc` on the computer, and exits bridge mode. Do not open the bridge menu manually before using a launcher. `list` only detects the card. `recover` attempts nested MIFARE Classic key recovery and saves a private `.mfd` dump in Downloads if it succeeds. `mfoc` needs a known key on the card and a susceptible card; success is not guaranteed.

Get the repository with `git clone https://github.com/nskge/bruce-pn532-field-kit.git`, or use GitHub's **Code > Download ZIP** and extract it. Open a terminal in the extracted `bruce-pn532-field-kit` folder before running the commands below.

## Windows

### Native Windows, without WSL

Download the `windows-nfc-host-tools` artifact from a successful run of the [Windows host-tools workflow](https://github.com/nskge/bruce-pn532-field-kit/actions/workflows/windows-host-tools.yml). GitHub may ask you to sign in to download an Actions artifact. Extract it so `dist\windows-host\nfc-list.exe` and `dist\windows-host\mfoc.exe` exist inside this checkout. The artifact is built from pinned upstream `libnfc` and `mfoc` sources; its folder includes licenses, source archives, and SHA-256 hashes. Alternatively, build it yourself using [the source-build instructions](platforms.md#native-windows-tools).

Open PowerShell in this checkout and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\nfc-windows.ps1 -Backend Native -Action List
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\nfc-windows.ps1 -Backend Native -Action Recover
```

If more than one ESP32-S3 serial device is connected, add `-ComPort COM7`, replacing `COM7` with the T-Embed's port shown in Device Manager. A successful recovery writes to your Windows Downloads folder.

### Windows with WSL

If Kali or Ubuntu is already installed in WSL, you can use it instead of the native host-tools artifact:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\nfc-windows.ps1 -Backend WSL -Distro kali-linux -Action List
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\nfc-windows.ps1 -Backend WSL -Distro kali-linux -Action Recover
```

Replace `kali-linux` with your WSL distribution name if different. The launcher installs missing Linux tools in WSL and forwards the Windows COM port to them. WSL itself must already be set up; this script does not install it. The dump still goes to Windows Downloads.

## Linux (Debian, Ubuntu, Kali, Fedora, Arch)

On Debian, Ubuntu, or Kali, first install Git, Python, and virtual-environment support:

```sh
sudo apt update
sudo apt install -y git python3 python3-venv
```

On Fedora or Arch, use your distribution's package manager for Git and Python. Then, from the repository folder:

```sh
./tools/nfc-unix.sh list
./tools/nfc-unix.sh recover
```

The launcher installs missing `libnfc`, `mfoc`, and Python `pyserial` using apt, dnf, or pacman. It creates a local `.venv-nfc` environment. If it cannot identify the serial port, inspect `ls -l /dev/serial/by-id/` and add `--port /dev/ttyACM0`, using the correct device path. If access is denied on Debian, add your user to `dialout` with `sudo usermod -aG dialout "$USER"`, then sign out and back in. Run the launcher as your normal user. The dump goes to `~/Downloads`.

For a detailed Debian test sequence, see [test from a clean computer](test-from-scratch.md).

## macOS

Install Homebrew and make sure `git` and `python3` are available. From the repository folder:

```sh
./tools/nfc-unix.sh list
./tools/nfc-unix.sh recover
```

The launcher uses Homebrew for missing `libnfc` and `mfoc` packages and installs `pyserial` in a local Python environment. It can autodetect one connected ESP32-S3; otherwise pass `--port /dev/cu.usbmodem...` with the actual path from `ls /dev/cu.*`. A successful recovery goes to `~/Downloads`. The macOS hardware path has not yet been validated on a device; report any setup error without posting a UID, card key, or dump.

## Common checks and limits

- Keep the card steady over the PN532 during `list` and `recover`.
- The launchers use guarded bridge mode for reading. Card writing requires a separate explicit `rfid bridge-write` command and a trusted host tool.
- If a host tool crashes and the T-Embed stays in bridge mode, press Back on the T-Embed or reset it.
- Keep `.mfd` dumps and recovered keys private. Restore any temporary card key from your private backup before using the card in its original application.
