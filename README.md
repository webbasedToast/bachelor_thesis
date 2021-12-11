# Bachelor Thesis #
## University project functioning as dircet follow up to the bachelor project ##

An [ESP](https://en.wikipedia.org/wiki/ESP32) based project to generate and record audio as a stimulus towards functional biometric authentication.

### Usability Requirements ###
To run the project sketch on an ESP32-based microcontroller it has to be flashed onto the controller memory.
For now the project is only tested using a [Heltec Wifi Kit 32](https://heltec.org/project/wifi-kit-32/).
The installation instructions for this controller into the arduino environment can be found [here](https://heltec.org/wifi_kit_install/).

Additional requirements to run [microphone](https://invensense.tdk.com/products/digital/inmp441/) or speaker are already given in the code of the sketch.
As speaker any mylar speaker can be used.

# Wiring of the Components #
How the different components are wired can be taken from the following schematic:
![Component wireing](https://raw.githubusercontent.com/webbasedToast/bachelor_project/master/Ausarbeitung/Media/Hardware_Pinout.png)

# Acknowledgements #
Credit for the MelodyPlayer package goes to Fabiano Riccardi.
The repository for the library can be reached [here](https://github.com/fabiuz7/melody-player-arduino)
