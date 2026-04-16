#include "BLEDeviceScanner.h"
#include <algorithm>

void BLEDeviceScanner::begin()
{
    BLEDevice::init(""); // Initialize without a specific local name

    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true); // Active scan gathers more data (like names)
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);  // High duty cycle for better detection
}

std::vector<PersonalDevice> BLEDeviceScanner::scan(uint32_t duration_secs) 
{
    std::vector<PersonalDevice> personal_devices;
    
    if (pBLEScan == nullptr)
        return personal_devices;

    BLEScanResults found_devices = pBLEScan->start(duration_secs, false);
    
    for (int i = 0; i < found_devices.getCount(); i++) {
        BLEAdvertisedDevice device = found_devices.getDevice(i);
        std::string manufacturer_name;
        DeviceType type;
        
        if (isLikelyPersonalDevice(&device, manufacturer_name, type)) {
            PersonalDevice pd;
            pd.mac = device.getAddress().toString();
            pd.name = device.haveName() ? device.getName() : "Unknown Device";
            pd.rssi = device.getRSSI();
            pd.manufacturer = manufacturer_name;
            pd.type = type;

            personal_devices.push_back(pd);
        }
    }
    
    pBLEScan->clearResults();
    
    return personal_devices;
}

static std::string toLower(const std::string &str) 
{
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    return lower_str;
}

bool BLEDeviceScanner::isLikelyPersonalDevice(BLEAdvertisedDevice *device, std::string &out_manufacturer, DeviceType &out_type) 
{
    out_manufacturer = "Unknown";
    out_type = DeviceType::UNKNOWN;
    
    if (device->haveName()) {
        std::string lower_name = toLower(device->getName());
        const std::vector<std::string> blocklist = {
            "tv", "television", "lg", "webos", "roku", "chromecast", "fire", 
            "speaker", "soundbar", "sonos", "fridge", "washer", "dryer", 
            "oven", "hue", "bulb", "lamp", "light", "govee", "vacuum", "robot"
        };
        
        for (const auto& keyword : blocklist)
            if (lower_name.find(keyword) != std::string::npos)
                return false; // matches appliance blocklist
    }

    if (device->haveAppearance()) {
        uint16_t appearance = device->getAppearance();
        
        if (appearance >= 192 && appearance <= 207) {
            out_manufacturer = "Generic Smartwatch";
            out_type = DeviceType::WEARABLE;
            return true;
        }

        if (appearance >= 960 && appearance <= 975) {
            out_manufacturer = "Generic Headphones/Earbuds";
            out_type = DeviceType::WEARABLE;
            return true;
        }

        if (appearance == 832 || appearance == 833) {
            out_manufacturer = "Generic Fitness/Heart Rate Sensor";
            out_type = DeviceType::WEARABLE;
            return true;
        }
    }

    if (device->haveManufacturerData()) {
        std::string md = device->getManufacturerData();
        if (md.length() >= 2) {
            uint16_t company_id = (md[1] << 8) | md[0];
            switch (company_id) {
            case 0x4C:
                out_manufacturer = "Apple";
                out_type = DeviceType::MOBILE;
                return true;
            case 0x06:
                out_manufacturer = "Microsoft";
                out_type = DeviceType::LAPTOP;
                return true;
            case 0x75:
                out_manufacturer = "Samsung";
                out_type = DeviceType::MOBILE;
                return true;
            case 0xE0:
                out_manufacturer = "Google";
                out_type = DeviceType::MOBILE;
                return true;
            }
        }
    }

    if (device->getAddressType() == BLE_ADDR_TYPE_RANDOM) {
        out_manufacturer = "Unknown (Randomized)";
        out_type = DeviceType::MOBILE;
        return true; 
    }

    return false; 
}
