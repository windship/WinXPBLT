# WinXPBLT
**Bluetooth BLE adapter with ESP32-S3-Zero for Windows XP**

It's really difficult to connect modern Bluetooth devices with Windows XP retro PC. Bluetooth dongles supports BLE are mainly over v5.0 and Windows 7 or later, so we can't find proper dongles/adapters for Windows XP.

Now you can make it by yourself with only one tiny ESC32-S3-Zero board. Nothing more required. Just flash *.ino file into the board with Arduino IDE, and connect it with a USB A to C cable to WinXP PC. That's all. Now you can connect and use Modern Bluetooth BLE keyboard and mouse.


**Features**

* Modern BLE devices available on Windows XP
* Up to 2 devices 
* Auto reconnect / Keep pairing even after PC reboot, switching pairing channel of multi-pairing device
* LED status indicator


**Library required**

* ESP32KeyBridge
* EspUsbDevice
* EspBle


**IDE Setting**

* Board : ESP32S3 Dev Module
* USB Mode : USB-OTG (TinyUSB)
* USB CDC On Boot : Disabled
* USB DFU On Boot : Disabled
* Upload Mode	: UART0 / Hardware CDC
* Flash Size : 4MB
* Partition Scheme : Default 4MB with spiffs
* PSRAM : OPI PSRAM


**LED Indicator Status**

* Blue : No device paired / scanning
* Yellow : 1 device paired
* Green : 2 devices paired
* Red : Security error
* Purple : BLE initialize error
