# The sniffer storing radiotap data and CSI
 
In this version the saving of pcap files with radiotap header is expanded bz separate task saving CSI into separate files.

## Configuration

All configuration can be done in the [config.h](main/config.h) file.

Mainly the Wi-Fi SSID and Password must be configured, since the ESP32 requires initial connection to the internet through Wi-Fi in order to download current time from NTP servers.

There are several things possible to configure:

- All channel scan, or only a single channel scanning.
- Filename mask of the output pcap files.
- Time period between saving files.
- MAC address filtering.