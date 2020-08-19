# Data Logging Wind Sensor and GPS

Teensy firmware is based off of https://github.com/MichaelStetner/TeensyDataLogger/blob/master/TeensyDataLogger.ino and writes wind sensor and GPS data to an SD card using the SDfat library. Tested for ~72 hours without issue. 

The python scripts provide analysis routines for loading the binary data, correcting missing GPS timestamps, and formatting the data as a pandas dataframe. 


Components for the full system:
* Teensy 3.5
* Anemoment trisonica weather station
* GPS: GPS Receiver - GP-20U7 (56 Channel) (sparkfun)
* Powering trisonica from USB: KeeYees 6pcs DC-DC Boost Converter 2V-24V to 5V-28V Adjustable 5V 9V 12V 24V Step Up Power Supply Module with Micro USB (amazon)
* Battery: 2-Pack Miady 10000mAh Dual USB Portable Charger
* SD card: SanDisk Ultra SDSQUNS-016G-GN3MN 16GB UHS-I Class 10 microSDHC