#pragma once
#include <NimBLEDevice.h>

#include <globals.h>
#include "core/display.h"

struct ForbiddenPacket {
    const char *pattern;
    const char *type;
};

class BLEWallOfFlipper {
public:
    BLEWallOfFlipper();
    static void loop();
private:
    unsigned long lastFlipperFoundMillis = 0; // Pour stocker le moment de la dernière annonce reçue
    bool isBLEInitialized = false;
    uint8_t sta_mac[6];
    int8_t found = 0;
    char strID[18];
    char strAddl[200];
    static const std::vector<ForbiddenPacket> forbiddenPackets;

    static bool matchPattern(const char *pattern, const uint8_t *payload, size_t length);

    class WOFDeviceCallbacks;

    static bool isMacAddressRecorded(const String &macAddress);

    static void recordFlipper(const String &name, const String &macAddress, const String &color, bool isValidMac);

    static void redrawMainBorder();
    void ble_scan_setup();
};
