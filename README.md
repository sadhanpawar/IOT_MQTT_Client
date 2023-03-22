# IOT PROJECT 1
The objective of this project is to create an MQTT client that utilizes the Ethernet stack code given in class. The implementation will be carried out on a TM4C123GXL board, which will be linked to an ENC28J60 Ethernet interface.

## Installation

### mosquitto 

```bash
sudo apt install mosquitto
sudo apt install mosquitto-clients
```

### Doxygen documentation

```bash
sudo apt install doxygen
sudo apt install graphviz
doxygen Doxyfile
```
### wireshark
[wireshark](https://www.geeksforgeeks.org/how-to-install-and-use-wireshark-on-ubuntu-linux/)

```bash
sudo dpkg-reconfigure wireshark-common => yes
sudo usermod -a -G wireshark $USER
sudo ethtool --offload enp7s0 tx off rx off
sudo apt install sendip
```
In wireshark:

Edit->prefereance->protocols->TCP->validate checksum

Edit->prefereance->protocols->UDP->validate checksum

Edit->prefereance->protocols->MQTT->validate checksum
### configuration file
```yaml
allow_ananymous true
listener 1883 0.0.0.0
password_file /etc/mosquitto/passwd
```

## Author
Sadhan Pawar Vadeher

UTA ID: 1002023295
