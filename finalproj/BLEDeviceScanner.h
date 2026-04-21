#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>
#include <string>

#define SCAN_DURATION 3 // in seconds, how long to scan for ads
#define SCAN_DUTY_CYCLE 99 // % of scanning interval taken by scanning window

struct Summary {
    unsigned short total;      // all devices detected
    unsigned short personal;   // devices that passed filter
    unsigned short mobiles;
    unsigned short laptops;
    unsigned short wearables;
    unsigned short unknowns;

    unsigned short apples;
    unsigned short googles;
    unsigned short microsofts;
    unsigned short samsungs;
    unsigned short others;
};

// Location ID -- change per deployment
#define LOCATION_ID 0x01  // 0x01=Rice Hall, 0x02=Olsson, 0x03=Thornton, etc.

class BLEDeviceScanner
{
    enum class DeviceType {
        MOBILE,
        LAPTOP,
        WEARABLE,
        UNKNOWN
    };

    enum class Manufacturer {
        APPLE,
        MICROSOFT,
        GOOGLE,
        SAMSUNG,
        OTHER
    };

public:
    void begin();
    Summary getStats(uint32_t duration_secs);

private:
    BLEScan *pBLEScan;
    static bool isLikelyPersonalDevice(BLEAdvertisedDevice *device, Manufacturer &out_manu, DeviceType &out_type);
};