/*
-------------------------------------------------------------------------
Datapak/Rampak Reader/Writer for Psion Organiser II
-------------------------------------------------------------------------
created by Martin Prest 2021

https://github.com/martinprest/Psion2-Datapak-Rampak-reader-writer

https://hackaday.io/project/176677-psion-ii-datapak-and-rampak-readerwriter

Uses linear or paged addressing, choose with paged_addr boolean variable below
packs that use segmented addressing are not supported
segmented addressing requires sending a segment address to the pack first

max read size is 64k bytes - could be increased, but packs larger than this generally use segmented addressing

v1.0 - April 2021 - reads Datapaks and Rampaks, writes to Rampaks
works with: PC_Psion2_datapak_read-write_v1_0.py on PC

v1.1 - June 2021 - added option for datapak write mode
works with: PC_Psion2_datapak_read-write_v1_1.py on PC

*/

// datapak pin connections on Arduino
const byte MR = 14;
const byte OE_N = 15;
const byte CLK = 16;
const byte SS_N = 17;
const byte PGM_N = 18;
const byte VPP = 19; // transistor switch for VPP supply
const byte data_pin[] = {2, 3, 4, 5, 6, 7, 8, 9}; // pins D0 to D7 on Datapak

const boolean paged_addr = true; // true for paged addressing, false for linear addressing

// DATAPAK PARAMETERS
// Datapaks contain an EPROM, so all bits start high, a write sets a bit low.
// Once low, a bit can only be taken high by an ultaviolet lamp. This erases all the data on the datapak and is referred to as formatting.
// The main power supply (VCC) of a PSION datapak EPROM is 5 V, so reads can be performed with a 5 V supply.
// To write, a nominal 21 V supply is used. I used 2x 9 V PP3 batteries, giving 18 V. 
// For the datapaks I was using this is reduced to an EPROM programming voltage (VPP) of about 13 V by a zener diode reference in the datapak circuit.
// Experience shows that a datapak write is achieved with a single 100 us pulse, so the code is more complex than it needs to be. 
// The PSION technical manual and EPROM datasheets suggest multiple short pulses are required,
// so options are provided for multiple write cycles and an additional overwrite after a verified write.
// There are many types of datapak, so you may need to adjust these parameters.
// Fine tuning of these parameters may also extend the life of the EPROM or speed up the write time.
// It would be best to open the pack, check the EPROM chip number and read the datasheet for that particular EPROM.
// The data verify mode referred to in EPROM datasheets is not possible as the datapak circuit does not allow OE_N low without SS_N low.
// So verification is done with VPP low and is the same as a standard read.

boolean datapak_mode = false; // true for datapaks, false for rampaks, mode can be changed by command option
boolean program_low = false; // indicates if PGM_N is low, during datapak write, so page counter can be pulsed accordingly
const boolean force_write_cycles = false; // set true to perform max write cycles, without break for confirmed write
const boolean overwrite = false; // set true to add a longer overwite after confirmed write
const byte max_datapak_write_cycles = 5; // max. no. of write cycle attempts before failure
const byte datapak_write_pulse = 100; // datapak write pulse in us, 1000 us = 1 ms, 10us write can be read by Arduino, but not Psion!

// ensure Baud rate matches the python PC software
//const long BaudRate = 9600; // default
//const long BaudRate = 19200; // faster
//const long BaudRate = 57600; // faster
const long BaudRate = 115200; // faster

const byte numFFchk = 3; // number of consecutive 0xFF bytes to signify the end of the pack, I tried 2 but then found a pack with 2x 0xFF bytes in it.

byte CLK_val = 0; // flag to indicate CLK state

//------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------

void delayShort() { // 1 us delay
  delayMicroseconds(1);
}

//------------------------------------------------------------------------------------------------------

void delayLong() { // 3 us delay
  delayMicroseconds(3);
}

//------------------------------------------------------------------------------------------------------

void ArdDataPinsToInput() { // set Arduino data pins to input
  // rampaks have pull down resistors in the pack circuit
  // datapaks do not have these, so internal Arduino input PULLUP resistors are enabled
  // datapaks seem to read without the PULLUPs, PULLUPs prevent garbage being read when there is no datapak connected
  // rampaks seem to read ok with PULLUPs enabled, but pullups & pulldowns will draw unnecessary current
  for (byte i = 0; i <= 7; i += 1) {
    if (datapak_mode) pinMode(data_pin[i], INPUT_PULLUP); // enable pullups if datapak
    else pinMode(data_pin[i], INPUT); // rampak has built in pull downs!!
  }
  delayShort();
}

//------------------------------------------------------------------------------------------------------

void ArdDataPinsToOutput() { // set Arduino data pins to output  
  for (byte i = 0; i <= 7; i += 1) {
    pinMode(data_pin[i], OUTPUT);
  }
  delayShort();
}

//------------------------------------------------------------------------------------------------------

byte readByte() { // Reads Arduino data pins, assumes pins are in the input state
  byte data = 0;
  for (int8_t i = 7; i >= 0; i -= 1) { // int8 type to allow -ve 8 bit numbers, so loop can end at -1
    data = (data << 1) + digitalRead(data_pin[i]); // read each pin and shift pin values into data, starting at MSB
  }
  return data;
}

//------------------------------------------------------------------------------------------------------

void writeByte(byte data) { // Writes to Arduino data pins, assumes pins are in the output state
  for (byte i = 0; i <= 7; i += 1) {
    digitalWrite(data_pin[i], data & 1); // write data bits to data_pin[i], starting at LSB 
    data = data >> 1; // shift data right, so can AND with 1 to read next bit
  }
}

//------------------------------------------------------------------------------------------------------

void resetAddrCounter() { // Resets pack counters
  digitalWrite(CLK, LOW); // start with clock low, CLK is LSB of address
  delayShort();
  CLK_val = 0; // set CLK state low
  digitalWrite(MR, HIGH); // reset address counter - asynchronous, doesn't require SS_N or OE_N
  delayShort();
  digitalWrite(MR, LOW);
  delayShort();
}

//------------------------------------------------------------------------------------------------------

void nextAddress() { // toggles CLK to advance the address, CLK is LSB of address and triggers the counter
  if (CLK_val == 0) {
    digitalWrite(CLK, HIGH);
    CLK_val = 1;
  }
  else if (CLK_val == 1) {
    digitalWrite(CLK, LOW);
    CLK_val = 0;
  }
  delayShort(); // settling time, let datapak catch up with address
}

//------------------------------------------------------------------------------------------------------

void nextPage() { // pulses PGM low, -ve edge advance page counter
  if (program_low) { // if PGM_N low, pulse high then low
  digitalWrite(PGM_N, HIGH);
  delayShort();
  digitalWrite(PGM_N, LOW);
  delayShort();
  }
  else { // if PGM_N high, pulse low then high
  digitalWrite(PGM_N, LOW); // -ve edge advances counter
  delayShort();
  digitalWrite(PGM_N, HIGH);
  delayShort();
  }
}

//------------------------------------------------------------------------------------------------------

void setAddress(word addr) { // resets counter then toggles counters to reach address, <word> so max address is 64k
  resetAddrCounter(); // reset counters
  byte page = (addr & 0xFF00) >> 8; // high byte of address
  byte addr_low = addr & 0xFF; // low byte of address
  if (paged_addr) { // if paged addressing
    for (byte p = 0; p < page; p++){nextPage();} // call nextPage, until page reached
    for (byte a = 0; a < addr_low; a++) {nextAddress();} // call nextAddress, until addr_low reached  
  }
  else { // else linear addressing
    for (word a = 0; a < addr; a++) {nextAddress();} // call nextAddress, until addr reached 
  }
  delayShort(); // extra delay, not needed?
}

//------------------------------------------------------------------------------------------------------

void packOutputAndSelect() { // sets pack data pins to output and selects pack memory chip (EPROM or RAM)
  digitalWrite(OE_N, LOW); // enable output - pack ready for read
  delayShort(); // delay whilst pack data pins go to output
  digitalWrite(SS_N, LOW); // take memory chip CE_N low - select pack
  delayShort();
  delayShort();
}

//------------------------------------------------------------------------------------------------------

void packDeselectAndInput() { // deselects pack memory chip and sets pack pins to input
  digitalWrite(SS_N, HIGH); // take memory chip select high - deselect pack
  delayShort();
  digitalWrite(OE_N, HIGH); // disable output - pack ready for write
  delayShort(); // don't do anything until output disabled & pack data pins become input
  delayShort();
}

//------------------------------------------------------------------------------------------------------

void printPageContents(byte page) { // set address to start of page and print contents of page (256 bytes) to serial (formatted with addresses)

  ArdDataPinsToInput(); // ensure Arduino data pins are set to input
  
  resetAddrCounter(); // reset address counters

  packOutputAndSelect(); // Enable pack data bus output, then select it

  Serial.println("addr  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  -------TEXT-------");
  Serial.println("------------------------------------------------------  01234567  89ABCDEF");

  for (byte p = 0; p < page; p++) { // page counter
    if (paged_addr) { // paged addressing
      nextPage(); // call nextPage(), until page reached      
    }
    else { // linear addressing
      for (byte a = 0; p <= 0xFF; p++) {
        nextAddress(); // call nextAddress() for every address in page, including 0xFF - so will advance to 0x100
      }
    }
  }
  for (word base = 0; base <= 255; base += 16) { // loop through 0 to 255 in steps of 16, last step to 256
    byte data[16];
    char str[19] = ""; // fill with zeros, 16+2+1, +1 for char zero terminator
    str[8] = 32; str[9] = 32; // gap in middle, 2 spaces
    byte gap = 0;
    for (byte offset = 0; offset <= 15; offset += 1) { // loop through 0 to 15
      if (offset >= 8) gap = 2; // jump gap for bytes 8 to 15
      byte pos = offset+gap; // string char position
      data[offset] = readByte(); // put byte in data array
      if ((data[offset] > 31) && (data[offset] < 127)) { // if printable char, put in str        
        str[pos] = data[offset];
      }
      else str[pos] = '.'; // else replace with '.'
      nextAddress();
    }
    char buf[80];
    sprintf(buf, "%04x  %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x  %s",
            base + page * 0x100, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
            data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15], str);
    Serial.println(buf);
  }
  packDeselectAndInput(); // deselect pack, then set pack data bus to input
}

//------------------------------------------------------------------------------------------------------

byte readAddr(word addr, bool output) { // set address, read byte, print if output true, and return value

  ArdDataPinsToInput(); // ensure Arduino data pins are set to input

  packOutputAndSelect(); // Enable pack data bus output, then select it

  setAddress(addr);

  byte dat = readByte(); // read Arduino data bus
  
  if (output == true) {
      char buf[15];
      sprintf(buf, "(Ard) %04x  %02x", addr, dat); // print to buf with 3 digits hex, then 2 digits hex, with leading zeros
      Serial.println(buf);
  }

  packDeselectAndInput(); // deselect pack, then set pack data bus to input

  return dat;
}

//------------------------------------------------------------------------------------------------------

word readAll(byte output) { // read all pack data, output if selected and return address of 1st free byte (with value 0xFF)
  // output: 0 - none, 1 - print address & byte value, 2 - send bytes 

  ArdDataPinsToInput(); // ensure Arduino data pins are set to input

  resetAddrCounter(); // reset counters

  packOutputAndSelect(); // Enable pack data bus output then select it

  if (output == 2) { // tell PC to read data
    Serial.println("XXRead"); // send "XXRead" to PC to tell it to receive read data
  }  

  bool quit = false;
  byte end_chk = 0;
  //byte numFFchk = 3; // now set globally at start of program
  byte addr_low = 0; // only goes to 255, then wraps around to zero
  word addr_tot = 0; // total
  while  (quit == false) {
    byte dat = readByte(); // read Datapak byte at current address
    if (output == 1) { // print to serial
      char buf[15];
      sprintf(buf, "(Ard) %04x: %02x", addr_low, dat); // print address and data
      Serial.println(buf);
    }
    if (output == 2) { // send to serial as data 
      Serial.write(dat); // send byte only
      unsigned long t = millis();
      while (!(Serial.available() > 0)) { // wait for data echo back, if no data: loop until there is, or timeout
        if (millis()-t > 1000) { // if timeout
          for (byte i = 0; i <= numFFchk; i++) { // tell PC to stop reading data by sending numFFchk 0xFF bytes
          Serial.write(0xFF);
          }
          Serial.println("(Ard) Timeout!");
          return false;
        }
      } // end of while loop delay for data echo back from PC
      byte datr = Serial.read(); // read byte value from serial
      if (datr != dat) { // if echo datr doesn't match dat sent
          for (byte i = 0; i <= numFFchk; i++) { // tell PC to stop reading data by sending numFFchk 0xFF bytes
          Serial.write(0xFF); 
          }
        Serial.println("(Ard) Read data not verified by PC!");
        return false;        
      }
    }
    if (dat == 0xFF) {
      end_chk++; // increase consecutive 0xFF count
    }
    else end_chk = 0; // reset 0xFF count if non 0xFF byte found
    if (end_chk == numFFchk) quit = true; // quit if numFFchk 0xFF bytes found, i.e. end of pack reached
    if ((paged_addr == true) && (addr_low == 0xFF)) { // if using paged addressing and end of page reached, go to next page, addr_low will wrap around to zero
      nextPage();
    }
    if (addr_tot >= 0xFFFF) quit = true; // break loop if reach max size: (65536-1 bytes) 64k !!
    if (quit != true) {      
    nextAddress();
    addr_low++;
    addr_tot++;
    }
  }

  packDeselectAndInput(); // deselect pack, then set pack data bus to input

  return addr_tot - (numFFchk - 1); // returns address of first 0xFF byte at end of pack
}

//------------------------------------------------------------------------------------------------------

bool writePakByteRampak(byte val) { // writes val to current address, returns true if written ok

  packDeselectAndInput(); // deselect pack, then set pack data bus to input (OE_N = high)

  ArdDataPinsToOutput(); // set Arduino data pins to output - can't do this with Datapak also output! OE_N must be high

  writeByte(val); // put value on Arduino data bus

  delayShort();

  digitalWrite(SS_N, LOW); // take CE_N low - select
  delayShort(); // delay for write
  digitalWrite(SS_N, HIGH); // take CE_N high - deselect
  delayShort(); 

  ArdDataPinsToInput(); // set Arduino data pins to input

  packOutputAndSelect(); // Enable pack data bus output then select it

  byte dat = readByte(); // read byte from datapak

  packDeselectAndInput(); // deselect pack, then set pack data bus to input

  if (dat == val) return true; // true if value written ok
  else return false; // false if write cannot be verified
}

//------------------------------------------------------------------------------------------------------

bool writePakByte(byte val, bool output) { // writes val to current address, returns false if write failed
// returns no. of cycles if write ok
// needs both PGM_N low and CE_N low for Eprom write

  byte write_cycles = 1;
  byte dat = 0;
  byte i = 1;

  if (datapak_mode) write_cycles = max_datapak_write_cycles;

  for (i = 1; i <= write_cycles; i++) {

    packDeselectAndInput(); // deselect pack, then set pack data bus to input (CE_N high, OE_N high)
  
    if (datapak_mode) {
      if (output) Serial.println("(Ard) Datapak write VPP on");
      digitalWrite(VPP, HIGH); // turn on VPP, 5V VCC is on already
      delayLong();      
    }
    else delayShort();
  
    ArdDataPinsToOutput(); // set Arduino data pins to output - can't do this with Datapak also output! OE_N must be high
  
    writeByte(val); // put value on Arduino data bus
    if (datapak_mode) delayLong();
    else delayShort();
  
    digitalWrite(SS_N, LOW); // take CE_N low - select
    
    if (datapak_mode) delayMicroseconds(datapak_write_pulse); // delay for write
    else delayShort(); 
  
    digitalWrite(SS_N, HIGH); // take CE_N high - deselect
    if (datapak_mode) delayLong();
    else delayShort();
  
    if (datapak_mode) {
      digitalWrite(VPP, LOW); // turn off VPP, 5V VCC goes off later
      delayLong();
      if (output) Serial.println("(Ard) Datapak write VPP off");
    }
  
    ArdDataPinsToInput(); // set Arduino data pins to input - for read
    
    packOutputAndSelect(); // Enable pack data bus output then select it
  
    dat = readByte(); // read byte from datapak
  
    if (output) {
      char buf[40];
      sprintf(buf, "(Ard) Cycle: %02d, Write: %02x, Read: %02x", i, val, dat);
      Serial.println(buf);
    }
    if ((!force_write_cycles) && (dat == val)) break; // written ok, so break out of for-loop
  }

  packDeselectAndInput(); // deselect pack, then set pack data bus to input (CE_N high, OE_N high)

  if ((overwrite) == true && (dat == val) && (datapak_mode)) { // for non-CMOS EPROM write again to make sure - overwrite
    if (output) Serial.println("(Ard) Datapak write VPP on");
    digitalWrite(VPP, HIGH); // turn on VPP, 5V VCC is on already
    delayLong();
    ArdDataPinsToOutput(); // set Arduino data pins to output - can't do this with Datapak also output! OE_N must be high
    writeByte(val); // put value on Arduino data bus
    delayLong();  
    digitalWrite(SS_N, LOW); // take CE_N low - select
    delayMicroseconds(datapak_write_pulse*3); // 3*delay for overwrite
    digitalWrite(SS_N, HIGH); // take CE_N high - deselect
    delayLong();  
    digitalWrite(VPP, LOW); // turn off VPP, 5V VCC goes off later
    delayLong();
    if (output) Serial.println("(Ard) Datapak write VPP off");
  }
  
  if (dat == val) return i; // return no. of cycles if value written ok
  else return false; // false if write cannot be verified
}

//------------------------------------------------------------------------------------------------------

bool writePakSerial(word numBytes) { // write PC serial data to pack
  
  bool done_w = false;
  word addr_tot = 0;
  byte addr_low = 0;

  if (datapak_mode) {
    digitalWrite(PGM_N, LOW); // take PGM_N low - select & program - need PGM_N low for CE_N low if OE_N high
    program_low = true;
  }
  resetAddrCounter(); // reset address counters, after PGM_N low
  
  for (addr_tot = 0; addr_tot < numBytes; addr_tot++) { // addr_tot will be 1 less than numBytes as addr starts from zero, numBytes starts from 1
    unsigned long t = millis();
    while (!(Serial.available() > 0)) { // if no data from PC, loop until there is, or timeout
      if (millis()-t > 1000) { // if timeout
        Serial.println("(Ard) Timeout!");
        return false;
      }
    }
    byte datw = Serial.read(); // read byte value from serial
    Serial.write(datw); // write data back to PC to verify and control data flow
    done_w = writePakByte(datw, /* output */ false); // write value to current memory address, no output because PC needs to verify data
    if (done_w == false) {
      Serial.println("(Ard) Write byte failed!");
      break;
    }
    if ((paged_addr == true) && (addr_low == 0xFF)) { // if using paged addressing and end of page reached, go to next page, addr_low will wrap around to zero
      nextPage();
    }
    nextAddress();
    addr_low++;
    }

  if (datapak_mode) {
    digitalWrite(PGM_N, HIGH); // take PGM_N high
    program_low = false;
  }
    
  if (done_w == true) {
  Serial.println("(Ard) Write done ok");
  }
  return done_w;
}

//------------------------------------------------------------------------------------------------------

void eraseBytes(word addr, word numBytes) { // erase numBytes, starting at addr
  setAddress(addr);
  bool done_ok = false;
  byte addr_low = addr & 0xFF; // low byte of addr
  Serial.print("(Ard) Erasing:");
  for (word i = 0; i <= numBytes; i++) { // loop through numBytes to write
    done_ok = writePakByte(0xFF, /* output */ false); // write 0xFF
    if (done_ok == false) {
      Serial.println("(Ard) Erase failed!");
      break; // break out of for loop
    }
    if (addr_low == 0xFF) { // if end of page reached, go to next page, addr_low will wrap around to zero
      if (paged_addr == true) nextPage(); // if using paged addressing go to next page
      Serial.print("."); // "." printed for each end of page
    }
    nextAddress(); 
    addr_low++;  
  }
  if (done_ok == true) {
  Serial.println("");
  Serial.println("(Ard) Erased ok");
  }
}

//------------------------------------------------------------------------------------------------------

void WriteMainRec(bool output) { // write record to main
  word endAddr = readAll(false); // false (0) - don't print output, just find 1st empty address
  if (datapak_mode) {
    digitalWrite(PGM_N, LOW); // take PGM_N low - select & program - need PGM_N low for CE_N low if OE_N high
    program_low = true;
  }
  if (readAddr(endAddr, /* output */ true) == 0xFF) { // move to start address & read it, if value is 0xFF write record
    char main[] = "--TEST"; // Main record text with leading "--" for length & identifier bytes
    byte len_main = sizeof(main)-1;// not including 0 at end
    main[0] = len_main-2; // record text length identifier byte
    main[1] = 0x90; // MAIN file identifier byte

    bool done_ok = false;
    int8_t cycles = 0; // can be -ve if write failed
    for (byte i = 0; i < len_main; i++) { // index starts from 0, so do while i < len_str, not i <= len_str
      char chr = main[i];
      cycles = writePakByte(chr, /* output */ true); // write i'th char of main record, cycles is no. of write cycles for EPROM
      if (cycles > 0) done_ok = true;
      if (output) {
        char buf[20];
        sprintf(buf, "(Ard) %04x: %02d %02x %c", i, cycles, chr, chr);
        Serial.println(buf);
      }    
      if (((endAddr & 0xFF) == 0xFF) & (paged_addr == true)) nextPage(); // if end of page reached, and paged_addr mode is true, go to next page
      nextAddress(); // next address
      endAddr++; // increment address pointer
      if (done_ok == false) break;
    }
    if (datapak_mode) {
      digitalWrite(PGM_N, HIGH); // take PGM_N high
      program_low = false;
    }
    if (done_ok == true) Serial.println("(Ard) add record done successfully");
    else Serial.println("(Ard) add record failed!");
  }
  else Serial.println("(Ard) no 0xFF byte to add record!");
}

void printCommands() {
  Serial.println("(Ard) Select a command:\ne - erase\nr - read pack\nw - write pack");
  Serial.println("0 - print page 0\n1 - print page 1\n2 - print page 2\n3 - print page 3");
  Serial.println("t - write TEST record to main\nm - datapak or rampak mode\n? - list commands\nx - exit");
}

void printMode() {
  if (datapak_mode) Serial.println("(Ard) Now in Datapak mode");
  else Serial.println("(Ard) Now in Rampak mode");
}

// ---------------------------------------------------------------------------
// setup starts here
// ---------------------------------------------------------------------------

void setup() {
  
  Serial.begin(BaudRate); // open serial, BaudRate is global const set at top of program
  
  //Serial.print("LED_BUILTIN is ");
  //Serial.println(LED_BUILTIN);

  pinMode(LED_BUILTIN, OUTPUT); // initialize digital pin LED_BUILTIN as an output  
  
  ArdDataPinsToInput(); // set Arduino data pins to input - default - but makes sure

  // set all control lines to input - pack will set default line states?
  pinMode(SS_N, INPUT); // deselect first
  pinMode(OE_N, INPUT);  
  pinMode(PGM_N, INPUT);
  pinMode(CLK, INPUT);
  pinMode(MR, INPUT);

  digitalWrite(LED_BUILTIN, HIGH); // turn the built in LED on to indicate waiting for Datapak & Enter

  printMode();

  // wait for connected datapak
  Serial.println("(Ard) Please connect Rampak/Datapak, then press Enter...");
  byte key;
  do {
    if (Serial.available()) {
      key = Serial.read();
      //char buf[15];
      //sprintf(buf, "(Ard) In: 0x%02x", byte(key));
      //Serial.println(buf);
    }
  } while (key != 10); // Enter

  digitalWrite(LED_BUILTIN, LOW); // turn the LED off when operating

  // set output pins

  pinMode(OE_N, OUTPUT);
  digitalWrite(OE_N, HIGH); // disable outputs, keep pak as inputs - high Z, doesn't matter if pack OE is output before, as SS_N is high, due to pull up in pack. Can't do before set to output because OE has pull-downs in pack

  delayShort(); // delay to ensure pack output is disabled before going further

  digitalWrite(SS_N, HIGH); // deselect - default pinMode is INPUT, setting this high enables INPUT pull-up, pull-up won't matter as SS_N also has pull-up in pack
  pinMode(SS_N, OUTPUT); // seems to sometimes clear data bits at address 0, maybe OUTPUT pulls low when initialised? Maybe need to take OE_N HIGH first?
  digitalWrite(SS_N, HIGH); // deselect - to make sure

  delayShort(); // delay to ensure pack is deselected before going further
 
  pinMode(PGM_N, OUTPUT);
  digitalWrite(PGM_N, HIGH); // (don't program, only read) take high, -ve edge advances page counter

  pinMode(CLK, OUTPUT);
  digitalWrite(CLK, LOW); // start with clock low, CLK is LSB of address

  pinMode(MR, OUTPUT);
  digitalWrite(MR, LOW); // don't reset clocks yet 

  printCommands();
}

// ---------------------------------------------------------------------------
// main loop starts here
// ---------------------------------------------------------------------------

void loop() { // command loop
  
  if (Serial.available() > 0) {
    char key = Serial.read();
    //char buf[15];
    //sprintf(buf, "(Ard) In: 0x%02x", byte(key)); // print input character
    //Serial.println(buf);
    
    switch (key) {    
      case 'e' : { // erase bytes
        if (!datapak_mode) {
        Serial.println("(Ard) Erase 512 bytes:"); // 512 bytes for first 2 pages
        eraseBytes(0,512); // starting from 0, erase bytes
        }
        else Serial.println("(Ard) Can't erase a Datapak! Use UV lamp, or a Rampak");
        break;
      }
      
      case 'w' : { // write pack from PC data
        bool write_ok = false;
        Serial.println("(Ard) Write Serial data to pack");
        char str[] = "XXWrite"; // Check for "XXWrite" from PC to indicate following data is write data
        if (Serial.find(str,7) == true) { // waits until timeout for Write to signal start of data
          byte numBytes[2]; // byte array to store pack image size
          byte bytes_read = Serial.readBytes(numBytes,2); // read 2 bytes for pack size, will need to be 3 if pack > 64kB !
          word numBytesTotal = (numBytes[0] << 8) + numBytes[1]; // shift 1st byte 8 bits left for high byte and add 2nd byte as low byte
          write_ok = writePakSerial(numBytesTotal); // write numBytes from file
          char buf[50];
          sprintf(buf, "(Ard) Pack size to write was: %04x bytes", numBytesTotal);
          Serial.println(buf);      
        }
        else Serial.println("(Ard) No XXWrite to begin data");
        if (write_ok == false) Serial.println("(Ard) Write failed!");        
        break;
      } 
      
      case 'r' : { // read pack and send to PC
        word endAddr = readAll(2); // 0 - no output, 1 - print data, 2 - dump data to serial
        char buf[40];
        sprintf(buf, "(Ard) Size of pack is: 0x%04x bytes", endAddr); // end address is same as size as address starts from 0, size starts from 1
        Serial.println(buf);
        break;
      }
      
      case '0': { // read page 0
        Serial.println("(Ard) Page 0:");
        printPageContents(0); // print zero page - first 256 bytes of datapak
        break;
      }
      
      case '1': { // read page 1
        Serial.println("(Ard) Page 1:");
        printPageContents(1); // print page 1 - second 256 bytes of datapak
        break;
      }
  
      case '2': { // read page 2
        Serial.println("(Ard) Page 2:");
        printPageContents(2); // print page 2 - third 256 bytes of datapak
        break;
      }
      
      case '3': { // read page 3
        Serial.println("(Ard) Page 3:");
        printPageContents(3); // print page 3 - fourth 256 bytes of datapak
        break;
      }
      
      case 't': { // add test record to main
        Serial.println("(Ard) add record to Main");
        WriteMainRec(true /*true for output */); // add a record to main
        break;
      }
      
      case 'm': {// toggle between datapak and rampak modes
        datapak_mode = 1-datapak_mode;
        printMode();
        break;
      }
  
      case '?': {// add test record to main
        printCommands();
        break;
      }
      
      case 'x': { // x to exit
        // exit - set all control lines to input - pack will set default line states
        pinMode(SS_N, INPUT); // deselect first
        pinMode(OE_N, INPUT);
        pinMode(PGM_N, INPUT);
        pinMode(CLK, INPUT);
        pinMode(MR, INPUT);
  
        ArdDataPinsToInput();
        
        Serial.println("(Ard) Please Remove Rampak/Datapak");
        Serial.println("XXExit"); // send exit command to PC
        do { // endless loop
          delay(1);
        } while (true);    
        break;
      }

      default: {
        Serial.println("(Ard) Command not recognised!");
        break;
      }
    }    
  }      
}
