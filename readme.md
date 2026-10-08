<h1 align="center">Hanshow Stellar L3N Electronic Shelf Label / AirTag Firmware</h1>

### Supported Model L3N@ (Note: Only adapted for the L3N@ 2.9" device; other models from the original project may no longer be compatible)

### Final Result

- [Web image upload](https://dene-.github.io/stellar-L3N-etag/)  
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

### Project Build Using Docker (ARM, etc)

Run `./build_docker.sh`, and wait for the output in `/Firmware` folder.

### Firmware layout

`Firmware/src` is layered; dependencies only point inwards:

| Directory | Contains | May include |
| --- | --- | --- |
| `domain/` | Rules and rendering: panel catalog, refresh policy, slideshow schedule, calendar, scenes and canvas | `domain/` only, no SDK |
| `application/` | Use cases: display session, scenes, image upload, settings, telemetry, status LED | `domain/`, its own `ports/` headers |
| `application/ports/` | What the use cases need from hardware (panel, clock, image storage, settings storage, battery, LED, telemetry sink) | |
| `infrastructure/` | SDK adapters implementing the ports: EPD drivers, flash storage, clock, LED, battery, NFC, UART | everything inwards + SDK |
| `ble/` | GATT table and the RxTx, raw EPD and OTA services, translating writes into use cases | everything inwards + SDK |
| `main.c` | Boot, wiring and the main loop | everything |

`domain/` and `application/` build without the Telink SDK. `python3 tools/firmware_tests/run.py` compiles them with the host `cc` against fake ports and runs their tests.

### Bluetooth Connection and OTA Update

- 1. You must disconnect the TTL TX line first, otherwise Bluetooth will not connect.
- 2. OTA update: use "Flash firmware" in the web tool. The update is applied when the device drops the connection; if it is still connected 45 s after the final command, the log says so and nothing was flashed.
- Firmware built before April 2026 compares the final command's CRC against the wrong buffer and never self-updates; the web tool works around it automatically, so no UART reflash is needed.

### Upload Images

- 1. Open the web tool (deployed to GitHub Pages from `main`), or run it locally: `cd web_tools && npm install && npm run dev`.
- 2. Connect via Bluetooth on the page (Chrome/Edge; Web Bluetooth needs https or localhost).
- 3. Select and upload an image; you can then add text, draw manually, or choose a dithering algorithm.
- 4. Send to device and wait for the screen to refresh.

### Screens

Switch scenes from the web page (Scene buttons) or with BLE command `E1 <scene>`:

| Scene | Shows |
| --- | --- |
| 0 | Last uploaded image |
| 1 | Clock: large time, temperature, battery, date band |
| 2 | Dashboard (default): tag name, time, temperature, battery voltage, date band |
| 3 | Slideshow of uploaded images |

Until the time is set over BLE, the clock scenes show `--:--` and "Set time via Bluetooth". Clock scenes redraw once a minute, skip the refresh when nothing changed, use a partial refresh for black-only changes and a full one whenever red content changes (date band, low battery), plus a full one every 10 partial refreshes against ghosting. "Fast refresh" in the web tool drops those periodic full refreshes and makes requested redraws partial; red changes and images are always shown with a full refresh, since a partial one cannot draw red.

Uploaded images (one image or a slideshow) are stored in MCU flash: at most 23, and within 212 KiB, which allows 22 on the 2.9" panel and 21 on the 1.54". Firmware before this release let the store run into the flash sectors holding the MAC address and the radio's crystal calibration. On the first boot of the new firmware, a store that overlapped them is deleted and the two sectors are reset to defaults; the tag then gets a new generated MAC (so a new `THX_…` name) once, and its images have to be uploaded again.

Scene code lives in `Firmware/src/domain/epd_scenes.c` (layouts) and `epd_canvas.c` (drawing). Text uses the Spleen bitmap font, converted pixel for pixel by `tools/fonts/gen_gfx_fonts.py`. Preview layouts on your computer without flashing: `python3 tools/scene_preview/preview.py` (needs `cc` and Pillow). It writes one PNG per panel size, scene and state plus a `sheet.png` overview to your temp dir, and exits non-zero if any text or shape is clipped or overflows its box.

### Display models

| `E0` model | Panel | Resolution | Colors | Controller family |
| --- | --- | --- | --- | --- |
| 0 | Auto-detect (default) | | | |
| 1 | BW213 | 250x128 | black/white | UC8151 |
| 2 | BWR213 (Stellar Pro 213R-N) | 250x128 | black/white/red | UC8151 |
| 3 | BWR154 | 200x200 | black/white/red | SSD16xx |
| 4 | 213ICE | 212x104 | black/white | SSD16xx |
| 5 | BWR290 / BWR296 (Stellar L3N@, 290R-N) | 296x128 | black/white/red | SSD16xx |
| 6 | BW290 / BW296 | 296x128 | black/white | SSD16xx |

Auto-detection only tells the two controller families apart. After a reset, the BUSY pin idles low on SSD16xx controllers and high on UC8151 ones, so it picks model 5 or model 2; no commands are sent to the panel. Black/white panels, the 2.13" ICE and the 1.54" need their model chosen in the web tool's display model selector (BLE `E0 <model>`); the choice is saved to flash. Black/white models draw the clock scenes without red, and uploaded images keep only their black plane. Images uploaded for one model are not shown after switching to a panel of another size; upload them again.

### Resolved / Pending Issues

- [X] Build errors
- [X] Flash not taking effect
- [X] Screen area incorrect / abnormal
- [X] Bluetooth cannot connect / Bluetooth OTA
- [X] Automatic model detection (controller family; black/white variants are selected manually)
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

### Original readme.md

[README_EN.md](/README_en.md) (For other models see the original project; this project only supports the L3N@ 2.9" device.)

> Note: Modified from [ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper).

### Reference Material

- [TLSR8359 Datasheet](/docs/DS_TLSR8359-E_Datasheet for Telink ULP 2.4GHz RF SoC TLSR8359.pdf)
- [TLSR8x5x BLE Development Handbook (Chinese)](/docs/Telink Kite BLE SDK Developer Handbook中文.pdf)
- [Display Driver Datasheet SSD1680.pdf](/docs/SSD1680.pdf)
