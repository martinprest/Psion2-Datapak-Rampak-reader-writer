# Psion2-Datapak-Rampak-reader-writer
<img align="left" src="Psion2_open_thumb.jpg" width="100">
The Psion Organiser II is a handheld 8-bit micro from the 1980s, it was sold to the public as a personal digital assistant (PDA) and was widely used in industry, over half a million were made. The PDA models have a diary, can store information and can be programmed to perform calculations and manipulate data, there are even some simple games for it. Its PDA functions have since been replaced by the smartphone, so alot of these Psions eventually found their way into the back of a drawer somewhere. However, these devices are as tough as a brick, so if you find one and put a fresh 9 V battery in, it's very likely to work. The same cannot be said of many devices that followed it. The Psion II was a ground-breaking device in its time, and it still has an active following of dedicated users. 

<br>Lots of info about the Psion II can be found at the website by Jaap Scherphuis: [Jaap's Psion2 site](https://www.jaapsch.net/psion/index.htm)

I used an Arduino to read and write to Psion II Datapaks and Rampaks, these packs contain a memory chip, two counters and some logic. The packs require a 5 V supply, so are well suited to an Arduino Nano which also uses 5 V, powered from USB. An optional higher voltage supply (I used 18V) plus a few components can be added to allow writing to Datapaks.

- Uses linear or paged addressing, larger segmented packs are not supported, maximum OPK file size is 64 kB.
- Rampaks can be read from or written to (bits changed from 1 to 0, or 0 to 1).
- Datapaks can be read from or written to, but not erased (bits from 1 to 0 only, because these packs contain EPROMs which require UV light to erase them).
- Pack image files for read/write use the standard OPK format of the Psion Developer software, for more info see [Martin Reid's Developer manual](https://sites.google.com/site/martin2reid/psion-organiser-ii/manuals/developer?authuser=0).

Psion Organiser II pack images can be created, viewed or edited using [Jaap's OPK editor](https://www.jaapsch.net/psion/opkedit.htm), which is at Jaap's webpage, but it can also be used offline.

I wrote software for the Arduino and PC. The PC software (in Python) allows pack images to be transferred between the PC and the Datapak/Rampak, via the Arduino USB serial port.

The circuitry is very simple, mostly just connecting of I/O lines to the datapak, I made a datapak connector which is just a 2x8 pin header, with stripboard to widen the connections to fit either side of the centre of a breadboard.

Be aware that you use this software and information at your own risk. Make sure you connect the pack the correct way around (see pinout below, also if you unclip the cover of the rampak/datapak some of these packs have pin 1 indicated by a red triangle) and only insert or remove a pack when prompted by the software. Be careful if you modify the software as it is possible to damage a datapak/rampak or the Arduino if both set the data pins to output at the same time. 

The software presents the user with a simple text menu of options. Sending a single character via the serial link will select the command.
Some of these commands can be used via the Arduino serial monitor, or similar terminal, but the read and write commands expect the data to be echoed back to verify it and control data flow, this is coded into the software. Filenames for transfer are entered directly into the Python code before it is run using the infile and outfile variables near the top of the program listing.

**Description of the the commands:**
- e - (rampaks only) erases the first 2 pages, i.e. the first 512 bytes of the pack, by setting all bits high. (full rampak formatting is best done using the Organiser in the normal way)
- r - reads data from the pack to the outfile on the PC.
- w - writes data from the PC infile to the pack. Modifies the pack ID bytes (to set as a rampack or adjust pack size) if certain flags are set in the Python program.
- 0, 1, 2 or 3 - (number n), prints the contents of page n (addresses: 0xn00 to 0xnFF) i.e. 256 bytes of the pack, as a hex dump.
- t - adds a test record to the "main" data file.
- m - swaps between rampak and datapak modes.
- l - swaps between linear and paged addressing modes.
- d - prints a directory of the pack contents.
- i - reads the id byte of the pack.
- b - checks to see if the pack is blank (datapaks need to be completely blank to write a new pack image).
- x - exits the menu and allows the pack to be removed.

# Components
- Arduino Nano or similar
- Header pins 2.54 mm pitch, 1x 2x8 pins and 2x 1x8 pins (used for datapak connector)
- Small piece of stripboard or perfboard (used for datapak connector)
- Psion organiser II Datapak or Rampak
- Breadboard
- for Datapak write: NPN transistor, PNP transistor, 3x resistors, 2x PP3 connectors, 2x PP3 9 V batteries

# Photo of the Datapak/Rampak reader-writer
<img src="Psion2_Rampak_read_write_1.jpg" width="600">

# Schematic
<img src="Psion2_datapak_read_write_schematic_v2.PNG" width="1200">
Add the optional VPP supply & control if you want to write data to a Datapak (not required for Rampaks). R1 limits the current load of the Arduino digital output (D19), Q1 (NPN) pulls the base of Q2 (PNP) low when the Arduino output goes high. R3 limits the current from Q2 emitter-base to Q1 collector-emitter. R2 is a pullup to keep Q2 in the off state until Q1 pulls low. The supply is 2x 9 V PP3 batteries. A zener diode reference in the datapak reduces VPP to the correct voltage for the EPROM, typically about 13 V.

# Datapak connector
This is made from headers soldered to stripboard to widen the connections for the breadboard, the centre line of the stripboard is cut to not short the connections.
The lower pins were pushed through the plastic to make them longer for soldering and the plastic was removed afterwards.<br>
<img src="datapak connector 1a.jpg" alt="connector from above" height="300">
<img src="datapak connector 2a.jpg" alt="connector from below" height="300">

# Datapak pinout
<img alt="Boris' pinout" src="datapak_pinout.PNG" width="600">
