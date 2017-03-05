# heaterControl
Couple of arduino sketckes to control a heater system at home

This project consist in two different parts:

Software
--------

Thermostat.ino

    Should be uploaded to an atmega328P. 
    Can be any arduino but for my project I used a bare atmega328P running at 8MHz and powered at 3.3V (see hardware part)
    
    Libraries needed:
    1- SPI (included on arduino IDE)
    2- RF24 (https://github.com/TMRh20/RF24)
    3- Lowpower (https://github.com/rocketscream/Low-Power)
    4- DS3231 (http://www.rinkydinkelectronics.com/library.php?id=73)
    5- PCD8544 (https://github.com/carlosefr/pcd8544/releases)
    
Receiver.ino

    Uploaded to another atmega328P
    
    Libraries needed:
    1- SPI (included on arduino IDE)
    2- RF24 (https://github.com/TMRh20/RF24)
    
There are some comments in the code, that can help to understand how they work.


Hardware
--------

Unit 1 (controller unit) consists in the following parts:

    1- Atmega328P microcontroller
    2- nokia 5110 lcd display
    3- nrf24l01+ module
    4- RTC DS3231
    5- thermistor 10K
    6- Battery holder (3xAAA in my case)
    7- voltaje regulator (I'm using a XC6206p332, but any low dropout regulator with los quiescent current is fine
    8- Resistors (2x 220 as pull-up for pushbuttons / 10K for RST / 10K  for the thermistor / 220 for the backlight)
    9- Capacitors (2x 100 nF decoupling in the VCC-GND of atmega / 100 nF for the rest line of the FTDI cable)
    10- Two push buttons
    11- Stripboard
    12- Cables and headers to connect all the components)
    13- A FTDI adaptor for programming if ussing a bare Atmega328p o arduino pro-mini.
    14- An enclosure to nicely fit all those componentes.
    
Unit 2 (receiver unit) consists in the following parts:

    1- Arduino pro-mini running a 8MHz and 3.3V.
    2- Relay module (5V controlled via transistor with an arduino pin (HIGH=3.3V).
    3- Led and resitor to show if heater is on or off
    4- Switch with three possitions (1- Auto mode / 2- Manual ON / 3- Manual OFF)
    5- Power suppply (5V 1A)
    6- Regulator (AMS1117-3.3V)
    7- nrf24l01+ module
    8- Cables and some connectors
    9- Terminal headers for the connection of the heater cables (always check the specification of the heater)
    10- An enclosure.
    
    
I provide also the fritzing file for the thermostat unit.
    
