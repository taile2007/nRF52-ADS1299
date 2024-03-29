This firmware is to connect nRF52 (Arm Cortex M4) from Nordic with ADS1299 (TI) - an AFE chip to measure biosignals with multiple channels. The chip is targeted for medical instrument application (EEG and ECG) with high precision (24-bit), simultaneous sampling and multichannel signal acquisition. The data rate
is configurable from 250 SPS to 16 kSPS for all eight channels with CMRR of 110dB.

The firmware code was comprised of three main parts:
- Connect the nRF52 SoC with the ADS chip
- Read data from the ADS chip using SPI communication protocol
- Transfer the data through Bluetooth Low Energy (BLE) to a mobile app

