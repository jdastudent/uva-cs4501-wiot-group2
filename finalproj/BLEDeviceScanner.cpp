#include "BLEDeviceScanner.h"
#include <algorithm>

void BLEDeviceScanner::begin()
{
    BLEDevice::init(""); // Initialize without a specific local name

    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true); // Active scan gathers more data (like names)
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(SCAN_DUTY_CYCLE); // High duty cycle for better detection
}

Summary BLEDeviceScanner::getStats(uint32_t duration_secs)
{
    Summary results = { 0 }; // all members start initialized 0
    if (pBLEScan == nullptr)
        return results;

    BLEScanResults found_devices = pBLEScan->start(duration_secs, false);

    results.total = found_devices.getCount();

    for (int i = 0; i < found_devices.getCount(); i++) {
        BLEAdvertisedDevice device = found_devices.getDevice(i);

        Manufacturer manufacturer_name;
        DeviceType type;
        if (isLikelyPersonalDevice(&device, manufacturer_name, type)) {
            results.personal++;

            switch (manufacturer_name) {
                case Manufacturer::APPLE:
                    results.apples++;
                    break;
                case Manufacturer::MICROSOFT:
                    results.microsofts++;
                    break;
                case Manufacturer::SAMSUNG:
                    results.samsungs++;
                    break;
                case Manufacturer::GOOGLE:
                    results.googles++;
                    break;
                case Manufacturer::OTHER:
                    results.others++;
            }

            switch (type) {
                case DeviceType::LAPTOP:
                    results.laptops++;
                    break;
                case DeviceType::MOBILE:
                    results.mobiles++;
                    break;
                case DeviceType::WEARABLE:
                    results.wearables++;
                    break;
                case DeviceType::UNKNOWN:
                    results.unknowns++;
            }
        }
    }

    pBLEScan->clearResults();
    return results;
}

static std::string toLower(const std::string &str)
{
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    return lower_str;
}

static double getShannonEntropy(const std::string& data)
{
    if (data.empty())
        return 0.0;

    std::array<int, 256> freq = {0};
    for (unsigned char c : data)
        freq[c]++;

    double entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (!freq[i])
            continue;

        double p = (double)freq[i] / data.size();
        entropy -= p * std::log2(p);
    }

    return entropy;
}

bool BLEDeviceScanner::isLikelyPersonalDevice(BLEAdvertisedDevice *device, Manufacturer &out_manu, DeviceType &out_type)
{
    // default assumptions
    out_manu = Manufacturer::OTHER;
    out_type = DeviceType::UNKNOWN;

    if (device->haveName()) {
        const std::vector<std::string> blocklist = {
            "tv", "television", "webos", "roku", "chromecast", "fire",
            "speaker", "soundbar", "sonos", "fridge", "washer", "dryer",
            "oven", "hue", "bulb", "lamp", "light", "govee", "vacuum", "robot"
        };

        std::string lower_name = toLower(device->getName());
        for (const auto& keyword : blocklist)
            if (lower_name.find(keyword) != std::string::npos)
                return false; // matches appliance blocklist
    }

    if (device->haveAppearance()) {
        uint16_t appearance = device->getAppearance();
        if (appearance >= 192 && appearance <= 207 ||
                appearance >= 960 && appearance <= 975 ||
                appearance == 832 ||
                appearance == 833) {
            out_type = DeviceType::WEARABLE;
            return true;
        }
    }

    if (device->haveServiceUUID()) {
        BLEUUID uuid = device->getServiceUUID();

        // 0xFD6F is the Google Exposure Notification system
        if (uuid.equals(BLEUUID((uint16_t)0xFD6F))) {
            out_manu = Manufacturer::GOOGLE;
            out_type = DeviceType::MOBILE;
            return true;
        }

        // 0x1805 is Current Time Service, 0x180F is Battery Service (Common on wearables)
        if (uuid.equals(BLEUUID((uint16_t)0x1805)) || uuid.equals(BLEUUID((uint16_t)0x180F))) {
            out_manu = Manufacturer::OTHER;
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
                    out_manu = Manufacturer::APPLE;
                    if (md.size() > 20 || getShannonEntropy(md) > 3)
                        out_type = DeviceType::MOBILE; // iPhones tend to have more complex data
                    else
                        out_type = DeviceType::LAPTOP; // Macs tend to have simpler and shorter data
                    return true;
                case 0x06:
                    out_manu = Manufacturer::MICROSOFT;
                    out_type = DeviceType::LAPTOP;
                    return true;
                case 0x75:
                    out_manu = Manufacturer::SAMSUNG;
                    out_type = DeviceType::MOBILE;
                    return true;
                case 0xE0:
                    out_manu = Manufacturer::GOOGLE;
                    out_type = DeviceType::MOBILE; // heuristic assumes BLE rarely enabled on chromebooks
                    return true;
            }
        }
    }

    // device passed blocklist w/ no manufacturer, but could be relevant device
    return true;
}