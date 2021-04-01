# Psion2-Datapak-Rampak-reader-writer

The Psion Organiser II is a handheld 8-bit micro from the 1980s, it was sold to the public as a personal digital assistant (PDA) and was widely used in industry, over 1/2 million were made. The PDA versions have a diary, can store information and can be programmed to perform calculations and manipulate data, there are even some simple games for it. Its PDA functions have since been replaced by the smartphone, so alot of these Psions eventually found their way into the back of a drawer somewhere. However, these devices are as tough as a brick, so if you find one and put a fresh 9 V battery in, it's very likely to work. The same cannot be said of many devices that followed it. The Psion II was a ground-breaking device in its time, and it still has an active following of dedicated users. Lots of info can be found on the wbsite by Jaap Scherphuis: [Jaap's Psion2 site](https://www.jaapsch.net/psion/index.htm)

Arduino and Python code to transfer psion2 pack images between a PC and an Arduino

I used an Arduino to read and write to a Psion II Rampak, these packs contain a memory chip, two counters and some logic. The packs require a 5 V supply, so are well suited to an Arduino Nano which also uses 5 V, powered from USB.

- Uses linear or paged addressing, larger segmented packs are not supported.
- Datapaks can be read from but not written to (because these packs contain EPROMs which require UV light to erase them and a 21 V supply to write).

I wrote software for the Arduino and PC. The PC software (in Python), allows pack images to be transferred between the PC and the Rampak, via the Arduino USB serial port.

The circuitry is very simple, just connecting of I/O lines to the datapak, I made a datapak connector which is just a 2x8 pin header, with stripboard to widen the connections to fit either side of the centre of a breadboard.

Psion Organiser II pack images can be created, viewed or edited using [Jaap's OPK editor](https://www.jaapsch.net/psion/opkedit.htm).

# Components
- 1× Arduino Nano or similar
- 1× Header pins 2x8 pins 2.54 mm pitch - used for datapak connector
- 1x small piece of stripboard - used for datapak connector
- 1× Psion organiser II Datapak or Rampak - Datapak for read or Rampak for read/write
- 1× Breadboard

# Photo of the Rampak reader-writer
<img src="Psion2_Rampak_read_write.jpg" width="600">

# Schematic
<img src="Psion2_datapak_read_write_schematic.PNG" width="600">

# Datapak connector
This is made from headers soldered to stripboard to widen the connections for the breadboard, the centre line of the stripboard is cut to not short the connections.
The lower pins were pushed deeper through the plastic to make them longer.
<img src="datapak connector 1a.jpg" alt="connector from above" height="300">
<img src="datapak connector 2a.jpg" alt="connector from below" height="300">
