# ESP32 Probe Request Sniffer

Simple sniffer of Probe Requests using ESP32 as a networking device with connected SD card as a storage for the collected Probe Requests.

The probe requests are stored in standard pcap file compatible with Wireshark or Scapy. 

The code on main branch of this repository was created to support scientific research into privacy in Wi-Fi networks. The research was published at international conference [IPIN2022](https://ipin-conference.org/2022/) in article: *What Your Wearable Devices Revealed About You and Possibilities of Non-Cooperative 802.11 Presence Detection During Your Last IPIN Visit* and is available in [IEEE Xplore digital library](https://doi.org/10.1109/IPIN54987.2022.9918134), [Zenodo](https://zenodo.org/record/7307613) or [arXiv](https://doi.org/10.48550/arXiv.2207.04706).

## Configuration

The ESP32 requires an initial connection to Wi-Fi network in order to dowwnload current time from NTP server and synchronize the internal clock with outside world. To do this, the SSID and password to nearby Wi-Fi must be set in the [config.h](main/config.h) file.

## Firmware Variants

There are several variants of the sniffer available in separate branches of this repository:

- **master**
    - The **master** branch contains the initial version, saving probe requests into pcap file on the SD card.

- **radiotap**
    - The **radiotap** branch expands on the master branch by also storing information about RSSI for each stored packet.

- **reduced_data**
    - The **reduced_data** branch removes the complexity of the master branch saving data into pcap files, and only stores the time, MAC address and RSSI into CSV file.

- **radiotap_and_csi**
    - The **radiotap_and_csi** branch contains the most complex solution to passive network monitoring, as it expands on the Probe Request collection with radiotap branch. In addition to the Probe Requests, it also collects CSI information in separate files.

## License

This code is in the Public Domain and the full license can be found in the [LICENSE](LICENSE) file.

## Citation

If you use this work, please cite one or more of the following:

    @misc{bravenec2022gitlabSniffer,
        author = {Bravenec, Tomáš},
        title = {{ESP32 Probe Sniffer}},
        year = 2022,
        url = {https://gitlab.com/tbravenec/esp32-probe-sniffer}
    }

    @inproceedings{bravenec2022your,
        author={Bravenec, Tomáš and Torres-Sospedra, Joaquín and Gould, Michael and Fryza, Tomas},
        booktitle={{2022 IEEE 12th International Conference on Indoor Positioning and Indoor Navigation (IPIN)}}, 
        title={{What Your Wearable Devices Revealed About You and Possibilities of Non-Cooperative 802.11 Presence Detection During Your Last IPIN Visit}}, 
        year={2022},
        doi={10.1109/IPIN54987.2022.9918134}
    }
