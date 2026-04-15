## zoneair-firmware

ESP32-S3 firmware for Zone Air WiFi-to-UART control of TCL/Pioneer mini-split AC units.
Target hardware: ESP32-S3 SuperMini (GPIO 17=RX, GPIO 18=TX, 8MB flash).

Build: `arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=custom,USBMode=hwcdc,CDCOnBoot=cdc --build-property "build.partitions=partitions" --build-property "upload.maximum_size=3145728" zoneair-firmware/`
