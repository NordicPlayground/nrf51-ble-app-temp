nrf51-ble-app-temp
==================

This project takes a temperature measurement using the internal temperature sensor of the nRF51822, does an ADC reading to get the battery voltage, packs it up as service data in an advertisement packet and sends it, along with an ID string based on the Bluetooth address of the chip. This is the format expected by the nRF Temp apps for iOS and Android, so these will be capable of picking this up and showing the measurement. 

Requirements
------------
- nRF5 SDK version 12.3.0
- One of the following evaluation/development kits
    - nRF51822 (PCA10001) with S130 version 2.0.1
    - nRF51422 (PCA10028) with S130 version 2.0.1
    - nRF52832 (PCA10040) with s132 version 3.0.0

The project may need modifications to work with later versions or other boards. 

To compile it, clone the repository in the examples/ble_peripheral/ folder in the nRF5 SDK.

Limitations
------------------
Battery measurements are currently not supported using nRF52 devices, and will always be
reported as 100%.

About this project
------------------
This project is a fork of https://github.com/NordicSemiconductor/nrf51-ble-app-temp 

The main purpose of the fork was to port it to a newer nRF5 SDK and add support for more devices and build environments.

It has not been thoroughly tested, so there might be unknown issues. It is provided as-is, without any warranty. 

The application is built to be used with the official nRF5 SDK, that can be downloaded from http://developer.nordicsemi.com/nRF5_SDK/
