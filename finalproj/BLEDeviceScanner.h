#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>
#include <string>

enum class DeviceType {
    MOBILE,
    LAPTOP,
    WEARABLE,
    UNKNOWN
};

struct PersonalDevice {
    std::string mac;
    std::string name;
    int rssi;
    std::string manufacturer; // e.g., "Apple", "Microsoft", "Unknown (Randomized)"
    DeviceType type;
};

class BLEDeviceScanner
{
public:
    void begin();
    std::vector<PersonalDevice> scan(uint32_t duration_secs);
private:
    BLEScan *pBLEScan;
    static bool isLikelyPersonalDevice(BLEAdvertisedDevice *device, std::string &out_manufacturer, DeviceType &out_type);
};
