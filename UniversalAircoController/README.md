# Hardware

Hardware used:
- [Universal Airco Controller v1.0 (ESP32-S3 based)](https://www.tinytronics.nl/en/development-boards/microcontroller-boards/with-wi-fi/universal-air-conditioning-controller-esp32-s3)
- 5-pin female-female Dupont connectors cable
- 90 degree male pin header, row of 5

## Installation

All the parts you need.
![Parts](Aircon1.png)

Locating the connector on the main board.
![Locating Connector](Aircon2.png)

Plug your connector into the CNS port.
![Connector Overview](Aircon3.png)

Route the cable behind the plastic cover.
![Routing Cable](Aircon4.png)

Further guide the cable.
![Routing Cable Part 2](Aircon5.png)

After reassembling the indoor unit plastic casing.
![Reassembled](Aircon6.png)

Connect the universal airco controller to the cable.
![Connecting Controller](Aircon7.png)

Place your controller somewhere safe in the unit.
![Final Result](Aircon8.png)

# Software
This hardware is only compatible with v4.2 and up, since the GPIO pins need to be set in the yaml file.

See sample.yml file for the example.
It is basically the simple.yml but with the pins set.