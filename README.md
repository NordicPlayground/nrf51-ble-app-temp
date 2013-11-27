nrf51-ble-app-temp
==================

This project takes a temperature measurement using the internal temperature sensor of the nRF51822, does an ADC reading to get the battery voltage, packs it up as service data in an advertisement packet and sends it, along with an ID string based on the Bluetooth address of the chip. This is the format expected by the nRF Temp apps for iOS and Android, so these will be capable of picking this up and showing the measurement. 

Requirements
------------
- nRF51 SDK version 4.4.2
- S110 SoftDevice version 5.2.1
- nRF51822 Evaluation Kit version 2.0.0

The project may need modifications to work with later versions. 

To compile it, clone the repository in the nrf51822/Board/pca10001/ble/ folder.

About this project
------------------
This application is one of several applications that has been built by the support team at Nordic Semiconductor, as a demo of some particular feature or use case. It has not necessarily been thoroughly tested, so there might be unknown issues. It is hence provided as-is, without any warranty. 

However, in the hope that it still may be useful also for others than the ones we initially wrote it for, we've chosen to distribute it here on GitHub. 

The application is built to be used with the official nRF51 SDK, that can be downloaded from www.nordicsemi.no, provided you have a product key for one of our kits.