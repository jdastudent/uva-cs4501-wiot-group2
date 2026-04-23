# CS 4501 Final Project
## Instructions for Heltec code:
1. Use the `lab6/LoRaWAN` project.
2. Add `BLEDeviceScanner.h` to the `include` folder, and `BLEDeviceScanner.cpp` to the `src` folder.
3. Replace `main.cpp` with your own device parameters that *The Things Network* gives you.
4. In `BLEDeviceScanner.h`, the macros that relate to scan duration and duty cycle.

### General high-level data flow procedure:
1. Collect device info seen as BLE ads. Ignore any non-personal devices such as TVs, projectors, smart appliances, etc. but keep data for laptops, wearables, mobile devices, etc.
2. Edit config in `BLEDeviceScanner.h`
3. Send data aggregates (along with possible control info) to *The Things Network* over LoRaWAN.
4. If # of personal devices exceeds fire code (defined as a constant), send an alert to backend over *The Things Network*.
5. Display a frontend website of the collected stats, along with a live panel of incoming alerts.

### Data sampling process:
All three boards will be deployed in parallel to the following buildings for 1 to 2 hours:
- Rice Hall
- Olsson Hall
- Thorton Hall (pick busiest representative wing, otherwise collection time explodes by fivefold)
- Mech Engr Bldg
- Jesser Hall
- Chem Engr Bldg
- Wilsdorf Hall
- Physics Bldg
- Chem Bldg
- Life Sciences Bldg
- Gilmer Hall
- APMA Small Hall
- AFC (extra, if we have more time)

All of these buildings are located around or in UVA's main STEM corridor:
<img width="868" height="723" alt="image" src="https://github.com/user-attachments/assets/49c9ec0a-53e8-4b6b-b89d-603017627447" />

### Data Format
Size refers to bytes.
Offset  Size  Field
0-1     2     total devices
2-3     2     personal devices
4-5     2     mobiles
6-7     2     laptops
8-9     2     wearables
10-11   2     unknowns
12-13   2     apples
14-15   2     googles
16-17   2     microsofts
18-19   2     samsungs
20      1     location_id
21      1     XOR checksum

### Other things to consider:
- get research on devices per person
- try to estimate actual # of people and compare with what BLE reports
