<h1 align="center">Hanshow Stellar L3N Electronic Shelf Label / AirTag Firmware</h1>

### Supported Model L3N@ (Note: Only adapted for the L3N@ 2.9" device; other models from the original project may no longer be compatible)

### Final Result

- [Web image upload](https://javabin.cn/stellar-L3N-etag/web_tools/)  
  ![Bluetooth Management](/images/web.jpg)
- Clock Mode 2, Image Mode  
  ![Clock Mode 2, Image Mode](/images/1553702163.jpg)

![Clock Mode 2, Image Mode](/images/1587504241.jpg)

### Flashing Steps

- 1. Remove the battery cover and check whether the PCB matches the photo below (or confirm the MCU is TLSR8359).

![Soldering Diagram](/USB_UART_Flashing_connection.jpg)

- 2. Solder four wires: GND, VCC, RX, RTS.
- 3. Use a USB-to-TTL module (CH340) to connect the four wires: RX -> TX, TX -> RX, VCC -> 3.3V, GND -> GND. Connect the RTS flying lead to pin 3 of the CH340G chip (optional; you can instead momentarily short it to GND before flashing).
- 4. Open https://atc1441.github.io/ATC_TLSR_Paper_UART_Flasher.html, keep baud rate at default 460800, Atime default, select file Firmware/ATC_Paper.bin.
- 5. Click Unlock, then Write to flash, and wait. On success the screen refreshes automatically.

### Project Build

```cmd
cd Firmware
makeit.exe clean && makeit.exe -j12
```

Sample successful build output:

```
'Create Flash image (binary format)'
'Invoking: TC32 Create Extended Listing'
'Invoking: Print Size'
"tc32_windows\\bin\\"tc32-elf-size -t ./out/ATC_Paper.elf
copy from `./out/ATC_Paper.elf' [elf32-littletc32] to `./out/../ATC_Paper.bin' [binary]
   text    data     bss     dec     hex filename
  75608    4604   25341  105553   19c51 ./out/ATC_Paper.elf
  75608    4604   25341  105553   19c51 (TOTALS)
'Finished building: sizedummy'
' '
tl_fireware_tools.py v0.1 dev
Firmware CRC32: 0xe62d501e
'Finished building: out/../ATC_Paper.bin'
' '
'Finished building: out/ATC_Paper.lst'
' '
```

### Bluetooth Connection and OTA Update

- 1. You must disconnect the TTL TX line first, otherwise Bluetooth will not connect.
- 2. OTA update: https://atc1441.github.io/ATC_TLSR_Paper_OTA_writing.html

### Upload Images

- 1. Run: `cd web_tools && python -m http.server`
- 2. Open http://127.0.0.1:8000 and connect via Bluetooth on the page.
- 3. Select and upload an image; you can then add text, draw manually, or choose a dithering algorithm.
- 4. Send to device and wait for the screen to refresh.

### Integrate with Apple Find My (AirTag Emulation)

- The device supports integration with Apple’s Find My network (it broadcasts a public key over Bluetooth per AirTag protocol; nearby Apple devices encrypt their location with that key and upload it; you can fetch and decrypt with your private key).
- This feature is disabled by default.
- To enable: in ble.c change the data after PUB_KEY= to your own public key. See (https://github.com/dchristl/macless-haystack or https://github.com/malmeloo/openhaystack) for how to obtain a key.
- Also set AIR_TAG_OPEN=1 in ble.c.

### Resolved / Pending Issues

- [X] Build errors
- [X] Flash not taking effect
- [X] Screen area incorrect / abnormal
- [X] Bluetooth cannot connect / Bluetooth OTA
- [ ] Automatic model detection
- [X] Python image generation script
- [X] Bluetooth image transfer size mismatch
- [X] Notify after Bluetooth image upload
- [X] Add scenes and support switching
- [X] Image mode
- [X] Web supports image switching
- [X] Added new time scene
- [X] Support setting year / month / day
- [X] Web supports drawing editor, direct upload, black & white dithering
- [X] Three-color dithering algorithm; device-side three-color display and Bluetooth transfer support
- [X] EPD buffer refresh occasional left/right black stripe issue
- [X] Chinese text display (some characters rendered as bitmaps; not all supported)
- [X] Support integration with Apple Find My (AirTag emulation)

### Original readme.md

[README_EN.md](/README_en.md) (For other models see the original project; this project only supports the L3N@ 2.9" device.)

> Note: Modified from [ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper).

### Reference Material

- [TLSR8359 Datasheet](/docs/DS_TLSR8359-E_Datasheet for Telink ULP 2.4GHz RF SoC TLSR8359.pdf)
- [TLSR8x5x BLE Development Handbook (Chinese)](/docs/Telink Kite BLE SDK Developer Handbook中文.pdf)
- [Display Driver Datasheet SSD1680.pdf](/docs/SSD1680.pdf)
