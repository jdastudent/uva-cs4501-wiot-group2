#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>
#include <string>

// Location ID -- change per deployment
/*
    0x1 = Rice Hall
    0x2 = Olsson Hall
    0x3 = Thornton Hall
    0x4 = Mech Engr Bldg
    0x5 = Jesser Hall
    0x6 = Chem Engr Bldg
    0x7 = Wilsdorf Hall
    0x8 = Physics Bldg
    0x9 = Chem Bldg
    0xA = Life Sciences Bldg
    0xB = Gilmer Hall
    0xC = APMA Small Hall
    0xD = AFC
*/
#define LOCATION_ID 0x6
#define SCAN_DURATION 10 // in seconds, how long to scan for ads
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
