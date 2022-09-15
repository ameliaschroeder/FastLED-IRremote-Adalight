# FastLED-IRremote-Adalight
Remote Controlled LED configuration using the FastLED library with a responsive backlight mode based off the Adalight library

This is code that I built to control the lighting of an Arcade cabinet I built, the goals were to make it remote controlled with a variety of color modes, the primary modes being a responsive light mode and a rainbow-cycling mode. 

Hardware Requirements:
- ESP32 (I recommend the Sparkfun esp32 thing because it has a voltage regulator and I managed to burn out two esp32's in the development process before I got one with a voltage regulator)
- Individually addressable LED strip (I used [these WS2815 strips](https://www.amazon.com/ALITOVE-Addressable-Programmable-Break-Point-Transmission/dp/B07R7T44L2/ref=sr_1_6?crid=1F778NNKH1GNM&keywords=ws2815&qid=1663275108&sprefix=ws2815%2Caps%2C90&sr=8-6) because I wanted the brightness of 12V strips, but WS2812b are super popular and work well)
- IR reciever
- IR remote [I used this one](https://www.amazon.com/SUPERNIGHT-Wireless-Dimmer-Controller-Replacement/dp/B09C1BFX48/ref=sr_1_7?keywords=LED+remote&qid=1663275776&sr=8-7) so all the buttons in the code correspond to these buttons but you can use pretty much any other remote and remap the IR commands to it
- *optional specific to 12V strips* [a Logic Shifter](https://www.digikey.com/en/products/detail/texas-instruments/SN74AHCT245N/277122) that translates the data signal from the 5v data pin on the board to the 12v data pin on the strip
- Power Supply (pick one specific to your light strip's power requirements)

*I'll add a wiring diagram later when I get a chance to make one* 

Software Requirements:
- [Prismatik](https://lightpack.tv/pages/downloads] controls the responsive light mode (Disclaimer: I've only ever been able to get Prismatic to function correctly on a Windows computer, they have a Mac version but I couldn't get it to work rip)

