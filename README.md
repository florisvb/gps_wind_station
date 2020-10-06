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

# Setting Trisonica settings via command line

The trisonica supports a simple command line interface using the following serial commands.

```
-----------------------------------------------------------------------------------------
help <command>              Display a list of CLI commands
exit                        Leave the CLI and return to sampling
nvwrite                     Write changes to non-volatile memory
baudrate [<baud>[now]]      Show/set the current baudrate
show [<parameter>]          Display parameters that can be shown; or show [] parameter
hide [<parameters>]         Display parameters that can be hidden; or hide [] parameter
display                     Display 
                            (name/what is tagged/what the tag is/significant figures/enabled/units)
outputrate [value]          Set/show the data output rate of the sampled data (Hz)
units [<parameter>[units]]  Sets/display the unit value for all adjustable parameters
compasscalibrate Yes        Start a compass calibration cycle
levelcalibrate Yes          Start a level calibration cycle
calibrate <temp> [<rh>]     Start a temp calibration cycle
decimals [<param>]          Set the number of decimal places of a Display parameter or a Group of Parameters
tag[<param>]                Display/set the ID Tag of a Display Parameter or a Group of Parameters
untag[<param>]              Remove the ID Tag of a Display Parameter or a Group of Parameters
-----------------------------------------------------------------------------------------
```
The Trisonica is capable of providing the following measurements.

```
-----------------------------------------------------------------------------------------
|     Name |            Description |  Tagged |      Tag | Decimals | Enabled |  Units  |
-----------------------------------------------------------------------------------------
|    IDTag |                 ID Tag |   Yes   |          |          |   Yes   |         |
|        S |          Wind Speed 3D |   Yes   |        S |     2    |   Yes   | m/s     |
|      S2D |          Wind Speed 2D |   Yes   |       S2 |     2    |   Yes   | m/s     |
|        D |   Horiz Wind Direction |   Yes   |        D |     0    |   Yes   | Degrees |
|       DV |    Vert Wind Direction |   Yes   |       DV |     0    |   Yes   | Degrees |
|        U |               U Vector |   Yes   |        U |     3    |   Yes   | m/s     |
|        V |               V Vector |   Yes   |        V |     3    |   Yes   | m/s     |
|        W |               W Vector |   Yes   |        W |     3    |   Yes   | m/s     |
|        T |            Temperature |   Yes   |        T |     2    |   Yes   | C       |
|       Cs |         Speed of Sound |   Yes   |        C |     2    |   Yes   | m/s     |
|   RHTemp |         RH Temp Sensor |   Yes   |     RHST |     2    |   Yes   | C       |
|       RH |     RH Humidity Sensor |   Yes   |     RHSH |     2    |   Yes   | %       |
|        H |               Humidity |   Yes   |        H |     2    |   Yes   | %       |
|       DP |               DewPoint |   Yes   |       DP |     2    |   Yes   | C       |
|    PTemp |   Pressure Temp Sensor |   Yes   |      PST |     2    |   Yes   | C       |
|        P |        Pressure Sensor |   Yes   |        P |          |   Yes   | hPa     |
|  Density |            Air Density |   Yes   |       AD |          |   Yes   | kg/m^3  |
|   LevelX |                Level X |   Yes   |       AX |          |   Yes   |         |
|   LevelY |                Level Y |   Yes   |       AY |          |   Yes   |         |
|   LevelZ |                Level Z |   Yes   |       AZ |          |   Yes   |         |
|    Pitch |                  Pitch |   Yes   |       PI |     1    |   Yes   | Degrees |
|     Roll |                   Roll |   Yes   |       RO |     1    |   Yes   | Degrees |
|    CTemp |           Compass Temp |   Yes   |       MT |     1    |   Yes   | C       |
|     MagX |              Compass X |   Yes   |       MX |          |   Yes   |         |
|     MagY |              Compass Y |   Yes   |       MY |          |   Yes   |         |
|     MagZ |              Compass Z |   Yes   |       MZ |          |   Yes   |         |
|  Heading |        Compass Heading |   Yes   |       MD |     0    |   Yes   | Degrees |
| TrueHead |           True Heading |   Yes   |       TD |     0    |   Yes   | Degrees |
-----------------------------------------------------------------------------------------
```


### Viewing the data in real-time

1. Check to see which port the Trisonica is connected to. For example, run `ls /dev` to see which devices are connected. I see the device as `tty.usbserial-D307LICF`.
2. From the terminal, run minicom: `minicom -s`. You should see something like this:
![](images/minicom.png?raw=true)
3. Down arrow key to `Serial port setup` and press `enter`
4. Press `A` to edit the serial device to match the device name found in step 1, press `enter`.
5. Press `F` to turn off hardware flow control
6. Press `esc` to return to main menu
7. Press `esc` to exit main menu. You should see a continuous feed of the measurements the sensor is making.

### Editing the settings

To set which of these parameters are delivered over serial, take the following steps:
1. Follow steps 1-7 from the section *Viewing the data in real-time*
2. Press `ctrl-c`, which should stop the scrolling of data and show a `>`
3. To see which parameters are enabled, decimals, and units, enter `display`
3. Set the desired parameters using the commands shown below. 
4. For example, if you want to show only these parameters: `S2D D Heading LevelX`, run the following sequence, pressing enter after each line. Unfortunately I believe you need to specify each parameter one at a time:
  * `hide all`
  * `show S2D`
  * `show D`
  * `show Heading`
  * `show LevelX` 
  * `nvwrite` (saves the parameter choices)
  * `exit`
  * Now you should see the stream of data for the parameters you selected.
  * Close the terminal when done. 

### Calibration

The following calibrations must be performed for each sensor:

1. Compass (performed in the field)
2. Tilt (performed in the lab)
3. Temperature (performed in lab)

The calibrations are performed as follows:

###### Compass

1. Enter `compasscalibrate YES` 
2. Rotate and tilt the unit in a three-dimensional figure-eight until calibration is finished
3. Enter `nvwrite` into terminal to save calibration


###### Tilt

1. Place TSM on known level surface and enter `levelcalibrate YES`
2. Wait until calibration is finished
3. Enter `nvwrite` into terminal to save calibration

###### Temperature

1. Loosely wrap TSM in coat/towel to create a zero-wind environment (e.g. the foam case)
2. Enter `calibrate <temp> [<rh>]` where temp is xx.x. in C and relative humidity is xx.x % (must know the calibration temp. If humidity is not supplied, then 50% is assumed.) There is a humidity sensor in our wind tunnel room. 
3. Wait until calibration is finished
4. Enter `nvwrite` into terminal to save calibration


### Default settings 

For our wind stations, these are the default settings that each sensor should be set to. Unfortunately I have not figured out how to script it successfully. May want to add `Pitch` and `Roll` for dynamic cases.
1. `baudrate 115200`
2. `outputrate 10`
3. `hide all`
4. `show S2D`
5. `show D`
6. `show U`
7. `show V`
8. `show W`
9. `show T`
10. `show H`
11. `show Density`
12. `show Heading`
13. `show LevelX`
14. `show LevelY`
15. `show LevelZ` 
16. `nvwrite`
17. Run all calibrations

### Scripting the default settings

There is a config file (turns hardware flow control off, that's it) and a script to set the default settings. 
Run the following from this home directory (may need to change device number). Generally seems like you need to run it two or three times before it takes.
Run the calibration seperately. 

`minicom defaults/minirc.trisonica.cfg -D /dev/tty.usbserial-D307LICF -S defaults/trisonica_wind_station_settings.txt`
