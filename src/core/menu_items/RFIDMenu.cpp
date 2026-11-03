#include "RFIDMenu.h"
#include "core/display.h"
#include "core/mifare_keys_manager.h"
#include "core/sd_functions.h"
#include "core/settings.h"
#include "core/serialcmds.h"
#include "core/utils.h"
#include "modules/rfid/PN532KillerTools.h"
#include "modules/rfid/amiibo.h"
#include "modules/rfid/chameleon.h"
#include "modules/rfid/pn532ble.h"
#include "modules/rfid/rfid125.h"
#include "modules/rfid/srix_tool.h" //added for srix Tool
#include "modules/rfid/tag_o_matic.h"

namespace {
void tryMifareFile(FS &fs) {
    if (&fs == &LittleFS && !setupLittleFS()) { displayError("Internal storage unavailable", true); return; }
    String path = loopSD(fs, true, "TXT|DIC|KEYS|CONF", "/BruceRFID");
    if (path.isEmpty()) return;
    std::set<String> candidates;
    size_t skipped = 0;
    if (!MifareKeysManager::readCandidates(fs, path, candidates, skipped) || candidates.empty()) {
        displayError("Invalid or empty key list", true);
        return;
    }
    TagOMatic(TagOMatic::READ_MODE, &candidates);
}

void importMifareWordlist(FS &fs) {
    if (&fs == &LittleFS && !setupLittleFS()) {
        displayError("Internal storage unavailable", true);
        return;
    }
    String path = loopSD(fs, true, "TXT|DIC|KEYS|CONF", "/BruceRFID");
    if (path.isEmpty()) return;
    size_t added = 0;
    size_t skipped = 0;
    if (!MifareKeysManager::importFromFile(fs, path, bruceConfig.mifareKeys, added, skipped)) {
        displayError("Could not import list (max 128 KiB / 4096 keys)", true);
        return;
    }
    displaySuccess("Imported " + String(added) + " keys; skipped " + String(skipped), true);
}

void mifareDictionaryMenu() {
    options = {
        {"Try common keys", []() { TagOMatic(TagOMatic::READ_MODE, false); }},
        {"Try saved list", []() { TagOMatic(TagOMatic::READ_MODE, true); }},
        {"Saved key count", []() {
            bruceConfig.ensureMifareKeysLoaded();
            displayInfo(String(bruceConfig.mifareKeys.size()) + " saved keys", true);
        }},
        {"Import internal", []() { importMifareWordlist(LittleFS); }},
        {"Try internal file", []() { tryMifareFile(LittleFS); }},
    };
    if (setupSdCard()) options.push_back({"Import from SD", []() { importMifareWordlist(SD); }});
    if (sdcardMounted) options.push_back({"Try SD file", []() { tryMifareFile(SD); }});
    loopOptions(options, MENU_TYPE_SUBMENU, "MIFARE keys");
}
} // namespace

#ifndef LITE_VERSION
#include "modules/rfid/emv_reader.hpp"
#endif
void RFIDMenu::optionsMenu() {
    options = {
#if !defined(REMOVE_RFID_HW_INTERFACE)  // Remove Hardware interface menu due to lack of external GPIO
        {"Read tag",    [=]() { TagOMatic(); }                     },
#ifndef LITE_VERSION
        {"Read EMV",    [=]() { EMVReader(); }                     },
        {"Read 125kHz", [=]() { RFID125(); }                       },
#endif
        {"Scan tags",   [=]() { TagOMatic(TagOMatic::SCAN_MODE); } },
        {"Load file",   [=]() { TagOMatic(TagOMatic::LOAD_MODE); } },
        {"Erase data",  [=]() { TagOMatic(TagOMatic::ERASE_MODE); }},
#endif
    };

#if !defined(REMOVE_RFID_HW_INTERFACE)
    // Emulate NDEF (arm the radio directly with a built NDEF message) only
    // makes sense on modules that can act as a target/card emulator.
    bool ndefEmulationSupported = bruceConfigPins.rfidModule == PN532_I2C_MODULE ||
                                  bruceConfigPins.rfidModule == PN532_SPI_MODULE ||
                                  bruceConfigPins.rfidModule == PN532_I2C_SPI_MODULE ||
                                  bruceConfigPins.rfidModule == ST25R3916_SPI_MODULE ||
                                  bruceConfigPins.rfidModule == ST25R3916_I2C_MODULE || false;
    if (ndefEmulationSupported) {
        options.push_back({"Emulate NDEF", [=]() { TagOMatic(TagOMatic::EMULATE_NDEF_MODE); }});
    }
    if (bruceConfigPins.rfidModule == PN532_I2C_MODULE || bruceConfigPins.rfidModule == PN532_SPI_MODULE) {
        options.push_back({"MIFARE keys", mifareDictionaryMenu});
        options.push_back({"PN532 USB bridge", [=]() {
            returnToMenu = false;
            if (!parseSerialCommand("rfid bridge", false)) {
                displayError("Could not start PN532 bridge");
                return;
            }
            displayInfo("PN532 USB bridge. Press Back to exit.");
            while (!returnToMenu && !check(EscPress)) vTaskDelay(pdMS_TO_TICKS(50));
            returnToMenu = true;
            vTaskDelay(pdMS_TO_TICKS(100));
        }});
    }
    options.push_back({"Write NDEF", [=]() { TagOMatic(TagOMatic::WRITE_NDEF_MODE); }});
#endif
#ifndef LITE_VERSION
    options.push_back({"Amiibolink", [=]() { Amiibo(); }});
#endif
    options.push_back({"Chameleon", [=]() { Chameleon(); }});
#ifndef LITE_VERSION
    options.push_back({"PN532 BLE", [=]() { Pn532ble(); }});
#if !defined(REMOVE_RFID_HW_INTERFACE) // Remove Hardware interface menu due to lack of external GPIO
    options.push_back({"PN532 UART", [=]() { PN532KillerTools(); }});
#endif
#endif
    options.push_back({"Config", [this]() { configMenu(); }});

#if !defined(REMOVE_RFID_HW_INTERFACE)
#ifndef LITE_VERSION
    if (bruceConfigPins.rfidModule == PN532_I2C_MODULE) {
        // Added SRIX Menu only if PN is set to i2c mode
        options.insert(options.begin() + 3, {"SRIX Tool", [=]() { PN532_SRIX(); }});
    }
#endif
#endif

    addOptionToMainMenu();

    vTaskDelay(pdMS_TO_TICKS(200));

    String txt = "RFID";
    if (bruceConfigPins.rfidModule == M5_RFID2_MODULE) txt += " (RFID2)";
#ifdef M5STICK
    else if (bruceConfigPins.rfidModule == PN532_I2C_MODULE) txt += " (PN532-G33)";
    else if (bruceConfigPins.rfidModule == PN532_I2C_SPI_MODULE) txt += " (PN532-G36)";
#else
    else if (bruceConfigPins.rfidModule == PN532_I2C_MODULE) txt += " (PN532-I2C)";
#endif
    else if (bruceConfigPins.rfidModule == PN532_SPI_MODULE) txt += " (PN532-SPI)";
    else if (bruceConfigPins.rfidModule == RC522_SPI_MODULE) txt += " (RC522-SPI)";
#if !defined(LITE_VERSION)
    else if (bruceConfigPins.rfidModule == ST25R3916_SPI_MODULE) txt += " (ST25R-SPI)";
    else if (bruceConfigPins.rfidModule == ST25R3916_I2C_MODULE) txt += " (ST25R-I2C)";
#endif
    loopOptions(options, MENU_TYPE_SUBMENU, txt.c_str());
}

void RFIDMenu::configMenu() {
    options = {
#if !defined(REMOVE_RFID_HW_INTERFACE)  // Remove Hardware interface menu due to lack of external GPIO
        {"RFID Module", setRFIDModuleMenu          },
#endif
        {"Add MIF Key", addMifareKeyMenu           },
        {"Back",        [this]() { optionsMenu(); }},
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "RFID Config");
}

void RFIDMenu::drawIcon(float scale) {
    clearIconArea();
    int iconSize = scale * 70;
    int iconRadius = scale * 7;
    int deltaRadius = scale * 10;

    if (iconSize % 2 != 0) iconSize++;

    tft.drawRoundRect(
        iconCenterX - iconSize / 2,
        iconCenterY - iconSize / 2,
        iconSize,
        iconSize,
        iconRadius,
        bruceConfig.priColor
    );
    tft.fillRect(iconCenterX - iconSize / 2, iconCenterY, iconSize / 2, iconSize / 2, bruceConfig.bgColor);

    tft.drawCircle(
        iconCenterX - iconSize / 2 + deltaRadius,
        iconCenterY + iconSize / 2 - deltaRadius,
        iconRadius,
        bruceConfig.priColor
    );

    tft.drawArc(
        iconCenterX - iconSize / 2 + deltaRadius,
        iconCenterY + iconSize / 2 - deltaRadius,
        2.5 * iconRadius,
        2 * iconRadius,
        180,
        270,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX - iconSize / 2 + deltaRadius,
        iconCenterY + iconSize / 2 - deltaRadius,
        2.5 * iconRadius + deltaRadius,
        2 * iconRadius + deltaRadius,
        180,
        270,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX - iconSize / 2 + deltaRadius,
        iconCenterY + iconSize / 2 - deltaRadius,
        2.5 * iconRadius + 2 * deltaRadius,
        2 * iconRadius + 2 * deltaRadius,
        180,
        270,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
}
