#include "rfid_commands.h"
#if !defined(LITE_VERSION)
#include "modules/rfid/ST25R3916.h"
#endif
#include "core/sd_functions.h"
#include "modules/rfid/PN532.h"
#include "modules/rfid/RFID2.h"
#include "modules/rfid/RFIDInterface.h"
#include <globals.h>

// Common workflows via serial:
//   Read a tag:                    rfid read [timeout=5000]
//   Emulate NDEF over ISO14443-4:  rfid emulate t4t [url|text <value>]
//   Emulate over FeliCa:           rfid emulate felica
//   Emulate from a saved file:     rfid loadfile [filepath] -> rfid emulate [t4t|felica]
//   Write NDEF to a tag:           rfid ndef [url|text] [value]
//   Write to a tag from a file:    rfid loadfile [filepath] -> rfid write [timeout=5000]

static RFIDInterface *_rfid = nullptr;
static bool _ensureRfid();
static bool _bridgeAllowCardWrites = false;

static bool _bridgeWaitReady(Adafruit_PN532 &nfc, uint32_t timeoutMs) {
    if (nfc._irq < 0) return nfc.waitready((uint16_t)(timeoutMs > 30000 ? 30000 : timeoutMs));
    uint32_t start = millis();
    while (digitalRead(nfc._irq) != LOW) {
        if (millis() - start > timeoutMs) return false;
        delay(1);
    }
    return true;
}

// Expose the embedded I2C PN532 as a PN532 UART-compatible reader.
// This lets libnfc use the built-in chip without changing its physical wiring.
static uint32_t rfidBridgeRun(bool allowCardWrites) {
    _bridgeAllowCardWrites = allowCardWrites;
    returnToMenu = false;
    if (bruceConfigPins.rfidModule != PN532_I2C_MODULE && bruceConfigPins.rfidModule != PN532_SPI_MODULE) {
        serialDevice->println("BRIDGE_ERROR: PN532 required");
        return false;
    }
    if (!_ensureRfid()) return false;
    PN532 *reader = static_cast<PN532 *>(_rfid);
    serialDevice->println("BRIDGE_READY");
    uint8_t frame[256];
    size_t used = 0;
    size_t exitMatched = 0;
    uint32_t lastByte = millis();
    static uint8_t ack[] = {0x00, 0x00, 0xff, 0x00, 0xff, 0x00};
    static constexpr char exitSequence[] = "\x1b" "BRUCE_EXIT\n";
    while (true) {
        if (returnToMenu) break;
        if (!serialDevice->available()) {
            if (used && millis() - lastByte > 1000) used = 0;
            delay(1);
            continue;
        }
        int next = serialDevice->read();
        if (next < 0) continue;
        if (used == 0 && (exitMatched || next == 0x1b)) {
            if (next == exitSequence[exitMatched]) {
                exitMatched++;
                if (exitMatched == sizeof(exitSequence) - 1) break;
            } else {
                exitMatched = 0;
            }
            continue;
        }
        lastByte = millis();
        if (used == 0 && next != 0x00) continue;
        if (used == 1 && next != 0x00) { used = 0; continue; }
        if (used == 2 && next == 0x00) continue; // retain the last two zeroes
        if (used == 2 && next != 0xff) { used = 0; continue; }
        frame[used++] = (uint8_t)next;
        if (used < 6) continue;
        uint8_t len = frame[3];
        if (len == 0 && frame[4] == 0xff) { used = 0; continue; } // host ACK
        if (len < 2 || len > 240 || (uint8_t)(len + frame[4]) != 0) { used = 0; continue; }
        if (used < (size_t)len + 7) continue;
        bool valid = frame[5] == 0xd4 && frame[len + 6] == 0;
        uint8_t checksum = 0;
        for (size_t i = 5; i < (size_t)len + 6; i++) checksum += frame[i];
        valid = valid && checksum == 0;
        uint8_t *command = frame + 6;
        uint8_t commandLen = len - 1;
        // Block plain card writes through InDataExchange. InCommunicateThru
        // carries encrypted nested-auth frames, whose first byte is arbitrary.
        if (valid && command[0] == 0x40) {
            size_t cardOp = 2;
            if (commandLen > cardOp) {
                uint8_t op = command[cardOp];
                if (!_bridgeAllowCardWrites &&
                    (op == 0xa0 || op == 0xa2 || op == 0xc0 || op == 0xc1 || op == 0xc2 || op == 0xb0)) {
                    valid = false;
                }
            }
        }
        used = 0;
        if (!valid) continue;
        reader->nfc.writecommand(command, commandLen);
        if (!_bridgeWaitReady(reader->nfc, 1000) || !reader->nfc.readack()) continue;
        serialDevice->write(ack, sizeof(ack));
        if (!_bridgeWaitReady(reader->nfc, 30000)) continue;
        uint8_t response[64] = {0};
        reader->nfc.readdata(response, sizeof(response));
        if (response[0] != 0 || response[1] != 0 || response[2] != 0xff) continue;
        size_t responseLen = (size_t)response[3] + 7;
        if (responseLen > sizeof(response)) continue;
        serialDevice->write(response, responseLen);
    }
    return true;
}

static uint32_t rfidBridgeCallback(cmd *) { return rfidBridgeRun(false); }
static uint32_t rfidBridgeWriteCallback(cmd *) { return rfidBridgeRun(true); }

static RFIDInterface *_createRfidModule() {
    switch (bruceConfigPins.rfidModule) {
        case PN532_I2C_MODULE: return new PN532(PN532::CONNECTION_TYPE::I2C);
#ifdef M5STICK
        case PN532_I2C_SPI_MODULE: return new PN532(PN532::CONNECTION_TYPE::I2C_SPI);
#endif
        case PN532_SPI_MODULE: return new PN532(PN532::CONNECTION_TYPE::SPI);
        case RC522_SPI_MODULE: return new RFID2(false);
#if !defined(LITE_VERSION)
        case ST25R3916_SPI_MODULE: return new ST25R3916(ST25R3916::SPI_MODE);
        case ST25R3916_I2C_MODULE: return new ST25R3916(ST25R3916::I2C_MODE);
#endif
        case M5_RFID2_MODULE:
        default: return new RFID2();
    }
}

static bool _ensureRfid() {
    if (_rfid != nullptr) return true;
    _rfid = _createRfidModule();
    if (!_rfid->begin()) {
        delete _rfid;
        _rfid = nullptr;
        serialDevice->println("ERROR: RFID module not found");
        return false;
    }
    return true;
}

static uint32_t _argTimeout(Command &cmd, uint32_t fallback = 5000) {
    String timeoutStr = cmd.getArgument("timeout").getValue();
    uint32_t timeout_ms = (uint32_t)timeoutStr.toInt();
    return timeout_ms == 0 ? fallback : timeout_ms;
}

static int _readTagWithTimeout(uint32_t timeout_ms) {
    returnToMenu = false;
    serialDevice->println("Waiting for tag (" + String(timeout_ms) + " ms)...");

    uint32_t deadline = millis() + timeout_ms;
    int result = RFIDInterface::TAG_NOT_PRESENT;

    while (millis() < deadline) {
        result = _rfid->read();
        if (result == RFIDInterface::SUCCESS) break;
        if (result == RFIDInterface::FAILURE) break;
        delay(50);
    }

    return result;
}

static void _printTagInfo() {
    serialDevice->println("UID:   " + _rfid->printableUID.uid);
    serialDevice->println("Type:  " + _rfid->printableUID.picc_type);
    serialDevice->println("SAK:   " + _rfid->printableUID.sak);
    serialDevice->println("ATQA:  " + _rfid->printableUID.atqa);
    serialDevice->println("Pages: " + String(_rfid->totalPages));
    if (_rfid->strAllPages.length() > 0) {
        serialDevice->println("--- data ---");
        serialDevice->print(_rfid->strAllPages);
        serialDevice->println("------------");
    }
}

// rfid read [timeout=5000]
uint32_t rfidReadCallback(cmd *c) {
    Command cmd(c);
    uint32_t timeout_ms = _argTimeout(cmd);

    if (!_ensureRfid()) return false;

    int result = _readTagWithTimeout(timeout_ms);
    if (result != RFIDInterface::SUCCESS) {
        serialDevice->println("Read failed: " + _rfid->statusMessage(result));
        return false;
    }

    _printTagInfo();
    return true;
}

// rfid write [timeout=5000]
uint32_t rfidWriteCallback(cmd *c) {
    Command cmd(c);
    uint32_t timeout_ms = _argTimeout(cmd);

    if (!_ensureRfid()) return false;
    if (_rfid->strAllPages.length() == 0) {
        serialDevice->println("No data to write. Run 'rfid read' first.");
        return false;
    }

    serialDevice->println("Place target tag on reader...");
    delay(timeout_ms > 3000 ? 2000 : 500);

    int result = _rfid->write();
    serialDevice->println("Write: " + _rfid->statusMessage(result));
    return result == RFIDInterface::SUCCESS;
}

// rfid clone [timeout=5000]
uint32_t rfidCloneCallback(cmd *c) {
    Command cmd(c);
    uint32_t timeout_ms = _argTimeout(cmd);

    if (!_ensureRfid()) return false;
    if (_rfid->printableUID.uid.length() == 0) {
        serialDevice->println("No UID loaded. Run 'rfid read' first.");
        return false;
    }

    serialDevice->println("Cloning UID: " + _rfid->printableUID.uid);
    serialDevice->println("Place Magic card on reader...");
    delay(timeout_ms > 3000 ? 2000 : 500);

    int result = _rfid->clone();
    serialDevice->println("Clone: " + _rfid->statusMessage(result));
    return result == RFIDInterface::SUCCESS;
}

// rfid emulate [t4t|felica] [value]
uint32_t rfidEmulateCallback(cmd *c) {
    Command cmd(c);
    String mode = cmd.countArgs() > 0 ? cmd.getArgument(0).getValue() : String("");
    mode.toLowerCase();

    if (!_ensureRfid()) return false;

    _rfid->emuMode = "";
    if (mode == "t4t" || mode == "felica") {
        _rfid->emuMode = mode;
        // For T4T allow an inline NDEF (URL by default, or "text <value>").
        if (mode == "t4t") {
            String type = "url";
            String value = "";
            int start = 1;
            if (cmd.countArgs() > 1) {
                String a1 = cmd.getArgument(1).getValue();
                String a1l = a1;
                a1l.toLowerCase();
                if (a1l == "text" || a1l == "url") {
                    type = a1l;
                    start = 2;
                }
            }
            for (int i = start; i < cmd.countArgs(); i++) {
                if (value.length() > 0) value += " ";
                value += cmd.getArgument(i).getValue();
            }
            if (value.length() > 0) { _rfid->buildNdefMessage(type, value); }
        }
    }

    if (_rfid->printableUID.uid.length() > 0) {
        serialDevice->println(
            "Emulating UID: " + _rfid->printableUID.uid + " (" + _rfid->printableUID.picc_type + ")"
        );
    }
    String caveat = _rfid->emulationCaveat();
    if (caveat.length() > 0) serialDevice->println("[!] " + caveat);
    serialDevice->println(
        "Starting RFID emulation" + (mode.length() ? (" (" + mode + ")") : String("")) + "..."
    );
    // backToMenu() (run after every serial command) leaves this flag set, which
    // would make emulate()'s loop bail out on its very first iteration.
    returnToMenu = false;
    int result = _rfid->emulate();
    serialDevice->println("Emulate: " + _rfid->statusMessage(result));
    return result == RFIDInterface::SUCCESS;
}

// rfid erase
uint32_t rfidEraseCallback(cmd *c) {
    if (!_ensureRfid()) return false;

    serialDevice->println("Place tag on reader to erase...");
    int result = _rfid->erase();
    serialDevice->println("Erase: " + _rfid->statusMessage(result));
    return result == RFIDInterface::SUCCESS;
}

// rfid info
uint32_t rfidInfoCallback(cmd *c) {
    if (!_rfid || _rfid->printableUID.uid.length() == 0) {
        serialDevice->println("No tag data. Run 'rfid read' first.");
        return false;
    }
    _printTagInfo();
    return true;
}

// rfid reset  — destroys the module instance (forces re-init on next command)
// rfid save [filename=rfid_dump] [format=bruce|flipper]
uint32_t rfidSaveCallback(cmd *c) {
    Command cmd(c);
    String filename = cmd.getArgument("filename").getValue();
    if (filename.length() == 0) filename = "rfid_dump";
    String format = cmd.getArgument("format").getValue();
    format.toLowerCase();

    if (!_ensureRfid()) return false;
    if (_rfid->printableUID.uid.length() == 0) {
        serialDevice->println("No tag data. Run 'rfid read' first.");
        return false;
    }

    int result;
    if (format == "flipper") {
        result = _rfid->saveFlipper(filename);
        if (result == RFIDInterface::NOT_IMPLEMENTED)
            serialDevice->println("Flipper format not supported by this module; using Bruce format.");
    } else {
        result = _rfid->save(filename);
    }
    serialDevice->println("Save: " + _rfid->statusMessage(result));
    return result == RFIDInterface::SUCCESS;
}

// rfid loadfile [filepath=/BruceRFID/rfid_dump.rfid]
uint32_t rfidLoadFileCallback(cmd *c) {
    Command cmd(c);
    String filepath = cmd.getArgument("filepath").getValue();
    if (filepath.length() == 0) filepath = "/BruceRFID/rfid_dump.rfid";

    if (!_ensureRfid()) return false;

    // loadFromFile() lives in the driver (or, by default, in RFIDInterface), so
    // the serial command and the GUI 'Load file' share one implementation and
    // never diverge in behavior.
    int loaded = _rfid->loadFromFile(filepath);
    serialDevice->println("Load: " + _rfid->statusMessage(loaded));
    if (loaded == RFIDInterface::SUCCESS) _printTagInfo();
    return loaded == RFIDInterface::SUCCESS;
}

// rfid ndef [url|text] [value]
uint32_t rfidNdefCallback(cmd *c) {
    Command cmd(c);
    String type = cmd.countArgs() > 0 ? cmd.getArgument(0).getValue() : String("url");
    String value = "";
    for (int i = 1; i < cmd.countArgs(); i++) {
        if (value.length() > 0) value += " ";
        value += cmd.getArgument(i).getValue();
    }
    type.toLowerCase();
    if (value.length() == 0) value = (type == "text") ? "Bruce" : "https://bruce.computer";

    if (!_ensureRfid()) return false;

    _rfid->buildNdefMessage(type, value);

    int result = _rfid->write_ndef();
    serialDevice->println("NDEF: " + _rfid->statusMessage(result));
    return result == RFIDInterface::SUCCESS;
}

uint32_t rfidResetCallback(cmd *c) {
    if (_rfid) {
        delete _rfid;
        _rfid = nullptr;
        serialDevice->println("RFID module reset.");
    } else {
        serialDevice->println("No active RFID module.");
    }
    return true;
}

// rfid autotest [mode=read-write] [timeout=5000]
uint32_t rfidAutotestCallback(cmd *c) {
    Command cmd(c);
    String mode = cmd.getArgument("mode").getValue();
    mode.toLowerCase();
    uint32_t timeout_ms = _argTimeout(cmd);

    if (!_ensureRfid()) return false;

    serialDevice->println("RFID autotest mode: " + mode);

    if (mode == "read") {
        int result = _readTagWithTimeout(timeout_ms);
        serialDevice->println("Read: " + _rfid->statusMessage(result));
        if (result == RFIDInterface::SUCCESS) _printTagInfo();
        return result == RFIDInterface::SUCCESS;
    }

    if (mode == "write") {
        int result = _rfid->write();
        serialDevice->println("Write: " + _rfid->statusMessage(result));
        return result == RFIDInterface::SUCCESS;
    }

    if (mode == "read-write") {
        int readResult = _readTagWithTimeout(timeout_ms);
        serialDevice->println("Read: " + _rfid->statusMessage(readResult));
        if (readResult != RFIDInterface::SUCCESS) return false;

        String savedPages = _rfid->strAllPages;
        _printTagInfo();
        serialDevice->println("Writing the captured dump back to the current tag...");
        int writeResult = _rfid->write();
        _rfid->strAllPages = savedPages;
        serialDevice->println("Write: " + _rfid->statusMessage(writeResult));
        return writeResult == RFIDInterface::SUCCESS;
    }

    if (mode == "clone") {
        int result = _rfid->clone();
        serialDevice->println("Clone: " + _rfid->statusMessage(result));
        return result == RFIDInterface::SUCCESS;
    }

    // MIFARE Classic: dict-attack dump (uses the normal read path, which
    // dispatches to the Crypto1 reader when the SAK is a MIFARE Classic SAK).
    if (mode == "mfc") {
        int result = _readTagWithTimeout(timeout_ms);
        serialDevice->println("MFC read: " + _rfid->statusMessage(result));
        if (result == RFIDInterface::SUCCESS) _printTagInfo();
        return result == RFIDInterface::SUCCESS;
    }

    if (mode == "mfc-write") {
        int result = _rfid->write();
        serialDevice->println("MFC write: " + _rfid->statusMessage(result));
        return result == RFIDInterface::SUCCESS;
    }

    if (mode == "mfc-clone") {
        int result = _rfid->clone();
        serialDevice->println("MFC clone: " + _rfid->statusMessage(result));
        return result == RFIDInterface::SUCCESS;
    }

    if (mode == "emulate") {
        if (_rfid->printableUID.uid.length() > 0) {
            serialDevice->println(
                "Emulating UID: " + _rfid->printableUID.uid + " (" + _rfid->printableUID.picc_type + ")"
            );
        }
        String caveat = _rfid->emulationCaveat();
        if (caveat.length() > 0) serialDevice->println("[!] " + caveat);
        // backToMenu() (run after every serial command) leaves this flag set, which
        // would make emulate()'s loop bail out on its very first iteration.
        returnToMenu = false;
        int result = _rfid->emulate();
        serialDevice->println("Emulate: " + _rfid->statusMessage(result));
        return result == RFIDInterface::SUCCESS;
    }

    serialDevice->println(
        "Invalid mode. Use: read, write, read-write, clone, emulate, mfc, mfc-write, mfc-clone"
    );
    return false;
}

void createRfidCommands(SimpleCLI *cli) {
    Command cmd = cli->addCompositeCmd("rfid,nfc");

    Command readCmd = cmd.addCommand("read", rfidReadCallback);
    readCmd.addPosArg("timeout", "5000");
    cmd.addCommand("bridge", rfidBridgeCallback);
    cmd.addCommand("bridge-write", rfidBridgeWriteCallback);

    Command writeCmd = cmd.addCommand("write", rfidWriteCallback);
    writeCmd.addPosArg("timeout", "5000");

    Command cloneCmd = cmd.addCommand("clone", rfidCloneCallback);
    cloneCmd.addPosArg("timeout", "5000");

    cmd.addBoundlessCommand("emulate", rfidEmulateCallback);
    cmd.addCommand("erase", rfidEraseCallback);
    cmd.addCommand("info", rfidInfoCallback);
    Command saveCmd = cmd.addCommand("save", rfidSaveCallback);
    saveCmd.addPosArg("filename", "rfid_dump");
    saveCmd.addPosArg("format", "bruce");
    Command loadFileCmd = cmd.addCommand("loadfile", rfidLoadFileCallback);
    loadFileCmd.addPosArg("filepath", "/BruceRFID/rfid_dump.rfid");
    cmd.addBoundlessCommand("ndef", rfidNdefCallback);
    cmd.addCommand("reset", rfidResetCallback);

    Command autotestCmd = cmd.addCommand("autotest,test", rfidAutotestCallback);
    autotestCmd.addPosArg("mode", "read-write");
    autotestCmd.addPosArg("timeout", "5000");
}
