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

v1.2 - Feb 2022 - fixed bug when reading pages with linear addressing
added read_fixed_size

v1.3 - Oct 2022 - added sizing and blank check using code from Matt Callow https://github.com/mattcallow/psion_pak_reader

*/

// datapak pin connections on Arduino
#define MR 14
#define OE_N 15
#define CLK 16
#define SS_N 17
#define PGM_N 18
#define VPP 19 // transistor switch for VPP supply
const byte data_pin[] = {2, 3, 4, 5, 6, 7, 8, 9}; // pins D0 to D7 on Datapak

boolean paged_addr = true; // true for paged addressing, false for linear addressing - note linear addressing is untested!! - paged is default

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

boolean datapak_mode = true; // true for datapaks, false for rampaks, mode can be changed by command option
boolean program_low = false; // will be set true when PGM_N is low during datapak write, so page counter can be pulsed accordingly
const boolean force_write_cycles = false; // set true to perform max write cycles, without break for confirmed write
const boolean overwrite = false; // set true to add a longer overwite after confirmed write
const byte max_datapak_write_cycles = 5; // max. no. of write cycle attempts before failure
const byte datapak_write_pulse = 100; // datapak write pulse in us, 1000 us = 1 ms, 10us write can be read by Arduino, but not Psion!

// ensure Baud rate matches the python PC software
//#define BaudRate 9600 // default
//#define BaudRate 19200 // faster
//#define BaudRate 57600 // faster
#define BaudRate 115200 // faster

word current_address = 0;
#define max_eprom_size 0x8000 // max eprom size - 32k - only used by Matt's code

boolean read_fixed_size = false; // true for fixed size
//boolean read_fixed_size = true; // true for fixed size
word read_pack_size = 0x7e9b; // set a fixed pack size for read
//word read_pack_size = 0x0100; 

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
    if (datapak_mode) pinMode(data_pin[i], INPUT_PULLUP); // enable pullups if datapak mode
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
  //delayLong();
  current_address = 0;
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
  current_address++;
  if (paged_addr && ((current_address & 0xFF) == 0)) nextPage(); // if paged mode and low byte of addr is zero (end of page) - advance page counter
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
  current_address = addr;
}

//------------------------------------------------------------------------------------------------------

void printPageContents(byte page) { // set address to start of page and print contents of page (256 bytes) to serial (formatted with addresses)

  ArdDataPinsToInput(); // ensure Arduino data pins are set to input
  packOutputAndSelect(); // Enable pack data bus output, then select it
  resetAddrCounter(); // reset address counter
  
  Serial.println(F("addr  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  -------TEXT-------"));
  Serial.println(F("------------------------------------------------------  01234567  89ABCDEF")); // comment out to save memory

  for (byte p = 0; p < page; p++) { // page counter
    if (paged_addr) { // paged addressing
      nextPage(); // call nextPage(), until page reached      
    }
    else { // linear addressing
      for (word a = 0; a <= 0xFF; a++) {
        nextAddress(); // call nextAddress() for every address in page, including 0xFF - so will advance to 0x100
      }
    }
  }
  for (word base = 0; base <= 255; base += 16) { // loop through 0 to 255 in steps of 16, last step to 256
    byte data;
    char str[19] = ""; // fill with zeros, 16+2+1, +1 for char zero terminator
    str[8] = 32; str[9] = 32; // gap in middle, 2 spaces
    byte pos = 0;
    char buf[6]; // buffer for sprintf: 5 chars + terminator
    sprintf(buf, "%04x ", base + page * 0x100); // format page in hex
    Serial.print(buf);
    for (byte offset = 0; offset <= 15; offset += 1) { // loop through 0 to 15
      if ((offset == 0) || (offset == 8)) Serial.print(" "); // at 0 or 7 print an extra spaces
      data = readByte(); // read byte from pack
      sprintf(buf, "%02x ", data); // format data byte in hex
      Serial.print(buf);
      if ((data > 31) && (data < 127)) { // if printable char, put in str        
        str[pos] = data;
      }
      else str[pos] = '.'; // else use '.'
      pos++;
      if (pos == 8) pos += 2; // jump 2 spaces after 8th char in str
      nextAddress();
    }
    Serial.print(" ");
    Serial.println(str);
    /*
    char buf[80]; // buffer for sprintf - used too much memory
    sprintf(buf, "%04x  %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x  %s",
            base + page * 0x100, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
            data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
    Serial.println(buf);*/
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

  word endAddr = read_dir(); // size pack - max is 64k
  Serial.print("Size: 0x");
  Serial.println(endAddr, HEX);

  ArdDataPinsToInput(); // ensure Arduino data pins are set to input
  packOutputAndSelect(); // Enable pack data bus output then select it
  resetAddrCounter(); // reset counters

  if (output == 2) { // tell PC to read data
    Serial.println(F("XXRead")); // send "XXRead" to PC to tell it to receive read data
    Serial.write(0); // size bytes, highest is zero, so max is 64k
    Serial.write(highByte(endAddr));
    Serial.write(endAddr & 0xFF);
  }  

  bool quit = false;
  word addr_tot = 0; // total
  while  (quit == false) {
    byte dat = readByte(); // read Datapak byte at current address
    if (output == 1) { // print to serial
      char buf[15];
      byte addr_low = addr_tot & 0xFF;
      sprintf(buf, "(Ard) %04x: %02x", addr_low, dat); // print address and data
      Serial.println(buf);
    }
    if (output == 2) { // send to serial as bytes
      Serial.write(dat); // send byte only
      unsigned long t = millis();
      while (!(Serial.available() > 0)) { // wait for data echo back, if no data: loop until there is, or timeout
        if (millis()-t > 1000) { // if timeout
          Serial.println(F("(Ard) Timeout!"));
          return false;
        }
      } // end of while loop delay for data echo back from PC
      byte datr = Serial.read(); // read byte value from serial
      if (datr != dat) { // if echo datr doesn't match dat sent
        delay(600); // delay to force timeout on PC !!
        Serial.println(F("(Ard) Read data not verified by PC!"));
        return false;        
      }
    }
    if (addr_tot >= endAddr) quit = true;
    if ((read_fixed_size == true) && (addr_tot >= read_pack_size)) quit = true; // quit if reach packSize and readFixedsize is true
    if (addr_tot >= 0xFFFF) quit = true; // break loop if reach max size: (65536-1 bytes) 64k !!
    if (quit != true) {      
    nextAddress();
    addr_tot++;
    }
  }

  packDeselectAndInput(); // deselect pack, then set pack data bus to input
  return addr_tot; // returns end address
}

//------------------------------------------------------------------------------------------------------

bool writePakByteRampak(byte val) { // writes val to current address, returns true if written ok - no longer used

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
      if (output) Serial.println(F("(Ard) Datapak write VPP on"));
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
      if (output) Serial.println(F("(Ard) Datapak write VPP off"));
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
    if (output) Serial.println(F("(Ard) Datapak write VPP on"));
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
    if (output) Serial.println(F("(Ard) Datapak write VPP off"));
  }
  
  if (dat == val) return i; // return no. of cycles if value written ok
  else return false; // false if write cannot be verified
}

//------------------------------------------------------------------------------------------------------

bool writePakSerial(word numBytes) { // write PC serial data to pack
  
  bool done_w = false;
  word addr = 0;

  if (datapak_mode) {
    digitalWrite(PGM_N, LOW); // take PGM_N low - select & program - need PGM_N low for CE_N low if OE_N high
    program_low = true;
  }
  resetAddrCounter(); // reset address counters, after PGM_N low
  
  for (addr = 0; addr <= numBytes; addr++) {
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
      Serial.println(F("(Ard) Write byte failed!"));
      break;
    }
    nextAddress();
    }

  if (datapak_mode) {
    digitalWrite(PGM_N, HIGH); // take PGM_N high
    program_low = false;
  }
    
  if (done_w == true) {
  Serial.println(F("(Ard) Write done ok"));
  }
  return done_w;
}

//------------------------------------------------------------------------------------------------------

void eraseBytes(word addr, word numBytes) { // erase numBytes, starting at addr - ony for rampaks
  setAddress(addr);
  bool done_ok = false;
  byte addr_low = addr & 0xFF; // low byte of addr
  Serial.print(F("(Ard) Erasing:"));
  for (word i = 0; i <= numBytes; i++) { // loop through numBytes to write
    done_ok = writePakByte(0xFF, false /* no output */); // write 0xFF
    if (done_ok == false) {
      Serial.println(F("(Ard) Erase failed!"));
      break; // break out of for loop
    }
    if (addr_low == 0xFF) { // if end of page reached, go to next page, addr_low will wrap around to zero
      Serial.print("."); // "." printed for each end of page
    }
    nextAddress(); 
    addr_low++;  
  }
  if (done_ok == true) {
  Serial.println("");
  Serial.println(F("(Ard) Erased ok"));
  }
}

//------------------------------------------------------------------------------------------------------

void WriteMainRec(bool output) { // write record to main
  word endAddr = read_dir();
  Serial.print(F("Pack size (from dir) is: "));
  Serial.println(endAddr, HEX);
  if (datapak_mode) {
    digitalWrite(PGM_N, LOW); // take PGM_N low - select & program - need PGM_N low for CE_N low if OE_N high
    program_low = true;
  }
  if (readAddr(endAddr, /* output */ true) == 0xFF) { // move to start address & read it, if value is 0xFF write record
    char main[] = "--The quick brown fox jumps over the lazy dog."; // Main record text with leading "--" for length & identifier bytes
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
      // if (((endAddr & 0xFF) == 0xFF) & (paged_addr == true)) nextPage(); // if end of page reached, and paged_addr mode is true, go to next page - moved to nextAddress()
      nextAddress(); // next address
      endAddr++; // increment address pointer
      if (done_ok == false) break;
    }
    if (datapak_mode) {
      digitalWrite(PGM_N, HIGH); // take PGM_N high
      program_low = false;
    }
    if (done_ok == true) Serial.println(F("(Ard) add record done successfully"));
    else Serial.println(F("(Ard) add record failed!"));
  }
  else Serial.println(F("(Ard) no 0xFF byte to add record!"));
}

void printCommands() {
  Serial.println(F("(Ard) datapak_read_write_v1.3"));
  printPackMode();
  printAddrMode();
  Serial.println(F("(Ard) Select a command:\ne - erase\nr - read pack\nw - write pack"));
  Serial.println(F("0 - print page 0\n1 - print page 1\n2 - print page 2\n3 - print page 3"));
  Serial.println(F("t - write TEST record to main\nm - rampak (or datapak) mode\nl - linear (or paged) addressing"));
  Serial.println(F("i - print pack id byte flags\nd - directory and size pack\nb - check if pack is blank"));
  Serial.println(F("? - list commands\nx - exit"));
}

void printPackMode() {
  if (datapak_mode) Serial.println(F("(Ard) Now in Datapak mode (Arduino input pullups)")); // sets Arduino input pullups for datapack mode in ArdDataPinsToInput()
  else Serial.println("(Ard) Now in Rampak mode (No Arduino input pullups)"); // no Arduino input pullups
}

void printAddrMode() {
  if (paged_addr) Serial.println(F("(Ard) Now in paged addressing mode"));
  else Serial.println(F("(Ard) Now in linear addressing mode"));
}

//------------------------------------------------------------------------------------------------------
// pack sizing and id bytes - code originally from Matt Callow, but modified. https://github.com/mattcallow/psion_pak_reader
//------------------------------------------------------------------------------------------------------

byte read_next_byte() { // only used by Matt's code
  nextAddress();
  byte data = readByte();
  return data;
}

void incr_addr(uint16_t bytes) { // only used by Matt's code
  for (uint16_t i=0;i<bytes;i++) { // increase address while i < bytes
    nextAddress();
  }
}

void print_pak_id() { // the first 2 bytes on a pack are the id and size bytes. id gives info about the pack
  ArdDataPinsToInput(); // ensure Arduino data pins are set to input
  packOutputAndSelect(); // Enable pack data bus output, then select it
  resetAddrCounter();
  byte id = readByte();
  byte sz = read_next_byte();
  byte pack_size = sz * 8;
  packDeselectAndInput(); // deselect pack, then set pack data bus to input
  Serial.println();
  Serial.print("Id Flags: 0x");Serial.println(id, HEX);
  Serial.print("0: ");Serial.println((id & 0x01)?"invalid":"valid"); // print bit flag value using conditional operator: (condition) ? true : false
  Serial.print("1: ");Serial.println((id & 0x02)?"datapak":"rampak");
  Serial.print("2: ");Serial.println((id & 0x04)?"paged":"linear");
  Serial.print("3: ");Serial.println((id & 0x08)?"not write protected":"write protected");
  Serial.print("4: ");Serial.println((id & 0x10)?"non-bootable":"bootable");
  Serial.print("5: ");Serial.println((id & 0x20)?"copyable":"copy protected");
  Serial.print("6: ");Serial.println((id & 0x40)?"standard":"flashpak or debug RAM pak");
  Serial.print("7: ");Serial.println((id & 0x80)?"MK1":"MK2");
  Serial.print("Size: "); Serial.print(pack_size); Serial.println(" kB");
}

word read_dir() { // read filenames and size pack
    ArdDataPinsToInput(); // ensure Arduino data pins are set to input
    packOutputAndSelect(); // Enable pack data bus output, then select it
    resetAddrCounter();
    uint8_t id = 0; // datafile id
    Serial.println();
    Serial.println(F("ADDR   TYPE         NAME      ID    Del? SIZE"));
    incr_addr(9); // move past header to 10th byte
    while(current_address < max_eprom_size) {
      Serial.print("0x"); // print address (6 chars + space)
      if (current_address+1<0x10) Serial.print("0");
      if (current_address+1<0x100) Serial.print("0");
      if (current_address+1<0x1000) Serial.print("0");
      Serial.print(current_address+1, HEX);
      Serial.print(" ");
      
      char short_record[10] = "         "; // 9 spaces + terminator
      uint8_t rec_len = read_next_byte();
      if (rec_len == 0xff) {
        Serial.println(F("End of pack"));
        break;
      }
      uint16_t jump = rec_len; // for jump, reduces when bytes read
      uint16_t rec_size = rec_len; // for printing
      uint8_t rec_type = read_next_byte();
      if (rec_type == 0x80) {
        jump = (read_next_byte()<<8) + read_next_byte();
        Serial.print("Long record, length = 0x");
        Serial.println(jump, HEX);
      } 
      else {
        if (rec_len > 9) rec_len = 9; // read first 8 chars of short record for printing
        for (uint8_t i=0;i<=rec_len-1;i++) {
          short_record[i] = read_next_byte();
          jump--;
        }
         
        Serial.print("0x"); // print rec type (4 chars)
        if (rec_type < 0x10) Serial.print("0"); // pad with zero, if required
        Serial.print(rec_type, HEX);
        
        switch (rec_type & 0x7f) { // print type (9 chars)
          case 0x01:
            Serial.print(" [Data]  ");
            break;
          case 0x02:
            Serial.print(" [Diary] ");
            break;
          case 0x03:
            Serial.print(" [OPL]   ");
            break;
          case 0x04:
            Serial.print(" [Comms] ");
            break;
          case 0x05:
            Serial.print(" [Sheet] ");
            break;
          case 0x06:
            Serial.print(" [Pager] ");
            break;
          case 0x07:
            Serial.print(" [Notes] ");
            break;
          case 0x10 ... 0x7E :
            Serial.print(" [Rec]   ");// datafile record
            break;
          default:
            Serial.print(" [misc]  ");// unknown type
          }
          Serial.print(short_record);
          
          if (((rec_type & 0x7f) == 1) || ((rec_type & 0x7f) <= 7)){// filename, with id in last byte
            id = short_record[8];
            Serial.print("  0x"); // id (6 chars + 1 space)
            if (id<0x10) Serial.print("0"); // pad with zero, if required
            Serial.print(id, HEX); // print value in hex
            Serial.print(" ");
          }
          else Serial.print("      "); // record - (5 chars + 1 space) - one less char than filename
                  
          Serial.print((rec_type < 0x80) ? " Yes  " : " No   "); // deleted y/n? (6 chars)
  
          Serial.print("0x"); // length (6 chars)
          if (rec_size<0x10) Serial.print("0"); // pad with zero, if required
          if (rec_size<0x100) Serial.print("0"); // pad with zero, if required
          if (rec_size<0x1000) Serial.print("0"); // pad with zero, if required
          Serial.println(rec_size, HEX);
          //Serial.println();
      }
      incr_addr(jump);
    }
    packDeselectAndInput(); // deselect pack, then set pack data bus to input
    return current_address;
}

bool blank_check() {
  ArdDataPinsToInput(); // ensure Arduino data pins are set to input
  packOutputAndSelect(); // Enable pack data bus output, then select it
  resetAddrCounter();
  Serial.println("\nBlank Check in 1k chunks '.'-blank 'x'-not blank");
  bool blank = (readByte() == 0xff); // is 1st byte blank?
  bool blank_1k = blank; // is this 1k blank?
  //for (uint16_t i=1;(blank && (i <= max_eprom_size));i++) { // read bytes while blank or up to max_eprom_size
  for (uint16_t i=1;i <= max_eprom_size;i++) { // read bytes up to max_eprom_size 
    if (read_next_byte() != 0xff) {; // blank true if byte is 0xFF
      blank = false;
      blank_1k = false;
    }
    if ((i % 1024) == 0) { // every 1024 bytes print a dot (addr div 1024)
      Serial.print(blank_1k ? "." : "x"); // dot if blank, x if not, using conditional operator: (condition) ? true : false
      blank_1k = true; // reset blank for next 1k chunk
    }
  }
  Serial.print("\nIs pack blank? : ");
  Serial.println(blank ? "Yes" : "No");
  packDeselectAndInput(); // deselect pack, then set pack data bus to input
  return blank;
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

  // set all control lines to input - pack will set default line states
  pinMode(SS_N, INPUT); // deselect first
  pinMode(OE_N, INPUT);  
  pinMode(PGM_N, INPUT);
  pinMode(CLK, INPUT);
  pinMode(MR, INPUT);

  digitalWrite(LED_BUILTIN, HIGH); // turn the built in LED on to indicate waiting for Datapak & Enter

  printPackMode(); // datapak or rampak mode
  printAddrMode(); // paged or linear addressing

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

  pinMode(OE_N, OUTPUT); // set Ard pin to output - OE has pull-down in pack, setting OE_N high before pinMode would enable the Ard pull-up, and both pull-up & pull-down would give an ambiguous state
  digitalWrite(OE_N, HIGH); // disable outputs, keep pak as inputs - high Z, doesn't matter if pack OE is output before SS, as SS_N is high due to pull-up in pack

  delayShort(); // delay to ensure pack output is disabled before going further

  digitalWrite(SS_N, HIGH); // deselect - default pinMode is INPUT, setting this line high enables INPUT pull-up, pull-up won't matter as SS_N also has pull-up in pack
  pinMode(SS_N, OUTPUT); // sometimes cleared data bits at address 0 (with rampack) if done before OE_N high, maybe OE_N pulled low when initialised? - OE_N high is now before SS_N high
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
        if (Serial.find(str,7) == true) { // waits for "XXWrite" to signal start of data, or until timeout
          byte numBytes[2] = {0}; // byte array to store pack image size
          byte bytes_read = Serial.readBytes(numBytes,2); // read 2 bytes for pack size, will need to be 3 if pack > 64kB !
          if (bytes_read == 2) {
            word numBytesTotal = (numBytes[0] << 8) + numBytes[1]; // shift 1st byte 8 bits left for high byte and add 2nd byte as low byte
            write_ok = writePakSerial(numBytesTotal); // write numBytes from file
            char buf[30];
            sprintf(buf, "(Ard) Pack size to write was: %04x bytes", numBytesTotal);
            Serial.println(buf); 
          }
          else Serial.println(F("(Ard) Wrong no. of size bytes sent!"));    
        }
        else Serial.println(F("(Ard) No XXWrite to begin data"));
        if (write_ok == false) Serial.println(F("(Ard) Write failed!"));        
        break;
      } 
      
      case 'r' : { // read pack and send to PC
        word endAddr = readAll(2); // 0 - no output, 1 - print data, 2 - dump data to serial
        char buf[30];
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
        datapak_mode = 1-datapak_mode; // toggle datapak mode
        printPackMode();
        break;
      }

      case 'l': {// toggle between paged and linear addressing modes
        paged_addr = 1-paged_addr; // toggle addressing mode
        printAddrMode();
        break;
      }

      case 'i': {// read pack id byte and print flag values
        print_pak_id();
        break;
      }
      
      case 'd': {// read dir and size pack
        word pack_size = read_dir();
        Serial.print("pack size is: 0x");
        Serial.println(pack_size, HEX);
        break;
      }

      case 'b': {// check if pack is blank
        blank_check();
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
