# Layers (dependencies point inwards only):
#   domain/          pure rules and rendering, no SDK headers (also built on the host by tools/)
#   application/     use cases; reach hardware only through application/ports/*.h
#   infrastructure/  SDK and hardware adapters implementing the ports
#   ble/             GATT services translating BLE writes into use cases
#   main.c           composition root: boot, wiring and the main loop
OUT_DIR += /domain /application /infrastructure /infrastructure/epd /infrastructure/storage /ble

OBJS += \
$(OUT_PATH)/main.o \
$(OUT_PATH)/domain/battery.o \
$(OUT_PATH)/domain/calendar.o \
$(OUT_PATH)/domain/clock_calibration.o \
$(OUT_PATH)/domain/device_name.o \
$(OUT_PATH)/domain/epd_canvas.o \
$(OUT_PATH)/domain/epd_scenes.o \
$(OUT_PATH)/domain/firmware_image.o \
$(OUT_PATH)/domain/panel.o \
$(OUT_PATH)/domain/period.o \
$(OUT_PATH)/domain/refresh_policy.o \
$(OUT_PATH)/domain/slideshow.o \
$(OUT_PATH)/domain/temperature.o \
$(OUT_PATH)/domain/time_zone.o \
$(OUT_PATH)/application/device_settings.o \
$(OUT_PATH)/application/display.o \
$(OUT_PATH)/application/image_upload.o \
$(OUT_PATH)/application/local_time.o \
$(OUT_PATH)/application/screen.o \
$(OUT_PATH)/application/status_led.o \
$(OUT_PATH)/application/telemetry.o \
$(OUT_PATH)/infrastructure/battery.o \
$(OUT_PATH)/infrastructure/wall_clock.o \
$(OUT_PATH)/infrastructure/i2c.o \
$(OUT_PATH)/infrastructure/led.o \
$(OUT_PATH)/infrastructure/nfc.o \
$(OUT_PATH)/infrastructure/uart.o \
$(OUT_PATH)/infrastructure/epd/epd_panel.o \
$(OUT_PATH)/infrastructure/epd/epd_spi.o \
$(OUT_PATH)/infrastructure/epd/epd_ssd16xx.o \
$(OUT_PATH)/infrastructure/epd/epd_uc8151c.o \
$(OUT_PATH)/infrastructure/epd/epd_bw_213.o \
$(OUT_PATH)/infrastructure/epd/epd_bw_213_ice.o \
$(OUT_PATH)/infrastructure/epd/epd_bwr_154.o \
$(OUT_PATH)/infrastructure/epd/epd_bwr_213.o \
$(OUT_PATH)/infrastructure/epd/epd_bwr_296.o \
$(OUT_PATH)/infrastructure/storage/image_store.o \
$(OUT_PATH)/infrastructure/storage/settings_flash.o \
$(OUT_PATH)/ble/ble.o \
$(OUT_PATH)/ble/epd_service.o \
$(OUT_PATH)/ble/gatt.o \
$(OUT_PATH)/ble/ota_service.o \
$(OUT_PATH)/ble/rxtx_commands.o

# Each subdirectory must supply rules for building sources it contributes
$(OUT_PATH)/%.o: ./src/%.c
	@echo 'Building file: $<'
	@$(TC32_COMPILER_PATH)tc32-elf-gcc $(GCC_FLAGS) $(INCLUDE_PATHS) -c -o"$@" "$<"
