#include "ble_wof.h"
#include "core/utils.h"

#define SCAN_TIME     10       // Scan duration in seconds
#define SCAN_INTERVAL 100     // BLE scan interval
#define SCAN_WINDOW   99      // BLE scan window
#define CMD_DELAY     500     // UI delay after commands
#define UI_READ_DELAY 2000    // UI delay for read feedback

static NimBLEScan*   pBLEScan;
const std::vector<ForbiddenPacket> BLEWallOfFlipper::forbiddenPackets = {
    {"4c0007190_______________00_____", "APPLE_DEVICE_POPUP"  }, // not working
    {"4c000f05c0_____________________", "APPLE_ACTION_MODAL"  }, // refactored for working
    {"4c00071907_____________________", "APPLE_DEVICE_CONNECT"}, // no option on flipper app
    {"4c0004042a0000000f05c1__604c950", "APPLE_DEVICE_SETUP"  }, // working
    {"2cfe___________________________",
     "ANDROID_DEVICE_CONNECT"                                 }, // not working cant find raw data in sniff
    {"750000000000000000000000000000_", "SAMSUNG_BUDS_POPUP"  }, // refactored for working
    {"7500010002000101ff000043_______", "SAMSUNG_WATCH_PAIR"  }, //  working
    {"0600030080_____________________", "WINDOWS_SWIFT_PAIR"  }, //  working
    {"ff006db643ce97fe427c___________", "LOVE_TOYS"           }  // working
};

class BLEWallOfFlipper::WOFDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        String deviceColor = "Unknown";
        bool isValidMac = false;
        bool isFlipper = false;


        if (advertisedDevice->isAdvertisingService(BLEUUID("00003082-0000-1000-8000-00805f9b34fb"))) {
            deviceColor = "White";
            isFlipper = true;
        } else if (advertisedDevice->isAdvertisingService(
                       BLEUUID("00003081-0000-1000-8000-00805f9b34fb")
                   )) {
            deviceColor = "Black";
            isFlipper = true;
        } else if (advertisedDevice->isAdvertisingService(
                       BLEUUID("00003083-0000-1000-8000-00805f9b34fb")
                   )) {
            deviceColor = "Transparent";
            isFlipper = true;
        }

        if (isFlipper) {
            const String macAddress = advertisedDevice->getAddress().toString().c_str();
            if (macAddress.startsWith("80:e1:26") || macAddress.startsWith("80:e1:27") ||
                macAddress.startsWith("0C:FA:22")) {
                isValidMac = true;
            }

            String name = advertisedDevice->getName().c_str();

            if (options.size() < 250)
                options.push_back({name, [&](){ returnToMenu = false; }});
            else {
                Serial.println("Memory low, stopping BLE scan...");
                pBLEScan->stop();
            }
            recordFlipper(
                name, macAddress, deviceColor, isValidMac
            );
        }

        const std::string advData = advertisedDevice->getManufacturerData();
        if (!advData.empty()) {
            const auto payload = reinterpret_cast<const uint8_t *>(advData.data());
            const size_t length = advData.length();
            for (auto &packet : forbiddenPackets) {
                if (matchPattern(packet.pattern, payload, length)) {
                    if (options.size() < 250)
                        options.push_back({packet.type, [&](){ returnToMenu = false; }});
                    else {
                        Serial.println("Memory low, stopping BLE scan...");
                        pBLEScan->stop();
                    }
                    break;
                }
            }
        }
    }
};

void BLEWallOfFlipper::ble_scan_setup() {
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new WOFDeviceCallbacks());
    // Active scan uses more power, but get results faster
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(SCAN_INTERVAL);
    // Less or equal setInterval value
    pBLEScan->setWindow(SCAN_WINDOW);

    // Bluetooth MAC Address
    esp_read_mac(sta_mac, ESP_MAC_BT);
    sprintf(
        strID,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        sta_mac[0],
        sta_mac[1],
        sta_mac[2],
        sta_mac[3],
        sta_mac[4],
        sta_mac[5]
    );
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

BLEWallOfFlipper::BLEWallOfFlipper() {
    tft.setTextSize(1);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    delay(CMD_DELAY);
    options = {};
    ble_scan_setup();
    loop();
}

void BLEWallOfFlipper::redrawMainBorder() {
    drawMainBorder();
    tft.drawString("-=Wall of Flipper=-", (tftWidth/2) - ((18*6)/2), 12);
}

void BLEWallOfFlipper::loop() {
    bool loopScan = false;

    while (!check(EscPress)) {
        options.clear();
        redrawMainBorder();
        displayTextLine("Scanning...");

        // Perform a NimBLE-style scan
        NimBLEScanResults results = pBLEScan->start(SCAN_TIME, false);
        pBLEScan->stop();
        if (check(EscPress)) return;

        if (options.empty()) {
            displayTextLine("No Flipper found. Retry...");
            pBLEScan->clearResults();
            options.clear();
            if (loopScan) {
                continue;
            }
        }

        returnToMenu = false;
        options.push_back({"Scan Once", [&](){ returnToMenu = false; }});
        options.push_back({"Scan Continuous", [&](){ loopScan = true; }});
        options.push_back({"Main Menu",  [&](){ returnToMenu = true;  }});
        loopOptions(options);

        if (returnToMenu) return;

        pBLEScan->clearResults();
    }
}

bool BLEWallOfFlipper::matchPattern(const char *pattern, const uint8_t *payload, size_t length) {
    size_t patternLength = strlen(pattern);
    for (size_t i = 0, j = 0; i < patternLength && j < length; i += 2, j++) {
        char byteString[3] = {pattern[i], pattern[i + 1], 0};
        if (byteString[0] == '_' && byteString[1] == '_') continue;

        uint8_t byteValue = strtoul(byteString, nullptr, 16);
        if (payload[j] != byteValue) return false;
    }
    return true;
}

bool BLEWallOfFlipper::isMacAddressRecorded(const String &macAddress) {
    File file = SD.open("/WoF.txt", FILE_READ);
    if (!file) { return false; }
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.indexOf(macAddress) >= 0) {
            file.close();
            return true;
        }
    }

    file.close();
    return false;
}

void BLEWallOfFlipper::recordFlipper(const String &name, const String &macAddress, const String &color, bool isValidMac) {
    if (!isMacAddressRecorded(macAddress)) {
        File file = SD.open("/WoF.txt", FILE_APPEND);
        if (file) {
            String status =
                isValidMac ? " - normal" : " - spoofed"; // Détermine le statut basé sur isValidMac
            file.println(name + " - " + macAddress + " - " + color + status);
            Serial.println("Flipper saved: \n" + name + " - " + macAddress + " - " + color + status);
        }
        file.close();
    }
}
