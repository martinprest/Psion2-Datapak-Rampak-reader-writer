/*

-------------------------------------------------------------------------
Datapak/Rampak Reader/Writer for Psion Organiser II
-------------------------------------------------------------------------
created by Martin Prest 2021

https://github.com/martinprest/Psion2-Datapak-Rampak-reader-writer

Uses linear or paged addressing, choose with paged_addr boolean variable below
packs that use segmented addressing are not supported
segmented addressing requires sending a segment address to the pack first

max read size is 64k bytes - could be increased, but packs larger than this generally use segmented addressing

v1.0 -  reads Datapaks and Rampaks, writes to Rampaks
works with: PC_Psion2_datapak_read-write_v1_0.py on PC

*/

// datapak pin connections on Arduino
const byte MR = 14;
const byte OE_N = 15;
const byte CLK = 16;
const byte SS_N = 17;
const byte PGM_N = 18;
const byte data_pin[] = {2, 3, 4, 5, 6, 7, 8, 9}; // pins D0 to D7 on Datapak

const boolean paged_addr = true; // true for paged addressing, false for linear addressing

byte CLK_val = 0; // flag to indicate CLK state

///////////////////////////////////////////////////////////////////////////////////////////////////////

void delayShort() { // 1 us delay
  delayMicroseconds(1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void ArdDataPinsToInput() { // set Arduino data pins to input
  for (byte i = 0; i <= 7; i += 1) {
    pinMode(data_pin[i], INPUT);
  }
  delayShort();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void ArdDataPinsToOutput() { // set Arduino data pins to output  
  for (byte i = 0; i <= 7; i += 1) {
    pinMode(data_pin[i], OUTPUT);
  }
  delayShort();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

byte readByte() { // Reads Arduino data pins, assumes pins are in the input state
  byte data = 0;
  for (int8_t i = 7; i >= 0; i -= 1) { // int8 type to allow -ve 8 bit numbers, so loop can end
    data = (data << 1) + digitalRead(data_pin[i]); // read each pin and shift pin values into data
  }
  return data;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void writeByte(byte data) { // Writes to Arduino data pins, assumes pins are in the output state
  for (byte i = 0; i <= 7; i += 1) {
    digitalWrite(data_pin[i], data & 1); // write bit at LSB to data_pin[pin]
    data = data >> 1; // shift data right, so can AND with 1 to read next bit
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void resetAddrCounter() { // Resets pack counters
  digitalWrite(CLK, LOW); // start with clock low, SCK is LSB of address
  delayShort();
  CLK_val = 0; // clear CLK flag
  digitalWrite(MR, HIGH); // reset address counter - asynchronous, doesn't require SS_N or OE_N
  delayShort();
  digitalWrite(MR, LOW);
  delayShort();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////////

void nextPage() { // toggles PGM low, then high to advance page counter
  digitalWrite(PGM_N, LOW); // -ve edge advances counter
  delayShort();
  digitalWrite(PGM_N, HIGH);
  delayShort();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void setAddress(word addr) { // resets counter then toggles counters to reach address, <word> so max address is 64k
  resetAddrCounter(); // reset counters
  byte page = highByte(addr); // high byte of address
  byte addr_low = lowByte(addr); // low byte of address
  if (paged_addr) { // if paged addressing
    for (byte p = 0; p < page; p++){nextPage();} // call nextPage, until page reached
    for (byte a = 0; a < addr_low; a++) {nextAddress();} // call nextAddress, until addr_low reached  
  }
  else { // else linear addressing
    for (word a = 0; a < addr; a++) {nextAddress();} // call nextAddress, until addr reached 
  }
  delayShort(); // extra delay, not needed?
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void packOutputAndSelect() { // sets pack data pins to output and selects pack memory chip (EPROM or RAM)
  digitalWrite(OE_N, LOW); // enable output - pack ready for read
  delayShort(); // delay whilst pack data pins go to output
  digitalWrite(SS_N, LOW); // take memory chip CE_N low - select pack
  delayShort();
  delayShort();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void packDeselectAndInput() { // deselects pack memory chip and sets pack pins to input
  digitalWrite(SS_N, HIGH); // take memory chip select high - deselect pack
  delayShort();
  digitalWrite(OE_N, HIGH); // disable output - pack ready for write
  delayShort(); // don't do anything until output disabled & pack data pins become input
  delayShort();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void printPageContents(byte page) { // set address to start of page and print contents of page (256 bytes) to serial (formatted with addresses)

  ArdDataPinsToInput(); // ensure Arduino data pins are set to input
  
  resetAddrCounter(); // reset address counters

  packOutputAndSelect(); // Enable pack data bus output, then select it

  Serial.println("addr  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ------TEXT------");
  Serial.println("--------------------------------------------------------0123456789ABCDEF");

  for (byte p = 0; p < page; p++) { // page counter
    if (paged_addr) { // paged addressing
      nextPage(); // call nextPage(), until page reached      
    }
    else { // linear addressing
      for (byte a = 0; p <= 0xFF; p++) {
        nextAddress(); // call nextAddress() for every address in page
      }
    }
  }
  
  for (word base = 0; base <= 255; base += 16) { // loop through 0 to 255 in steps of 16, last step to 256
    byte data[16];
    char str[17];
    for (byte offset = 0; offset <= 15; offset += 1) { // loop through 0 to 15
      data[offset] = readByte(); // put byte in data array
      if ((data[offset] > 31) && (data[offset] < 127)) { // if printable char, put in str
        str[offset] = data[offset];
      }
      else str[offset] = '.'; // else replace with '.'
      nextAddress();
    }
    str[16] = 0; // end string indicator
    char buf[80];
    sprintf(buf, "%04x  %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x  %16s",
            base + page * 0x100, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
            data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15], str);
    Serial.println(buf);
  }
  packDeselectAndInput(); // deselect pack, then set pack data bus to input
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////////

word readAll(byte output) { // read all pack data, output if selected and return address of 1st free byte (with value 0xFF)
  // output: 0 - none, 1 - print address & byte value, 2 - send bytes 

  ArdDataPinsToInput(); // ensure Arduino data pins are set to input

  resetAddrCounter(); // reset counters

  packOutputAndSelect(); // Enable pack data bus output then select it

  if (output == 2) { // tell PC to read data
    Serial.println("XXRead"); // send "Read" to PC to tell it to receive read data
  }  

  bool quit = false;
  byte end_chk = 0;
  byte numFFchk = 3;
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
        if (millis()-t > 1000) { // check time
          for (byte i = 0; i <= numFFchk; i++) { // tell PC to stop reading data by sending numFFchk FF bytes
          Serial.write(0xFF);
          }
          Serial.println("(Ard) Timeout!");
          return false;
        }
      }
      byte datr = Serial.read(); // read byte value from serial
      if (datr != dat) {
          for (byte i = 0; i <= numFFchk; i++) { // tell PC to stop reading data by sending numFFchk FF bytes
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
    if (end_chk == numFFchk) quit = true; // quit if numFFend 0xFF bytes found, i.e. end of pack reached
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

///////////////////////////////////////////////////////////////////////////////////////////////////////

bool writePakByte(byte val) { // writes val to current address, returns true if written ok

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

///////////////////////////////////////////////////////////////////////////////////////////////////////

bool writePakSerial(word numBytes) { // write PC serial data to pack
  resetAddrCounter(); // reset address counters
  bool done_w = false;
  word addr_tot = 0;
  byte addr_low = 0;
  for (addr_tot = 0; addr_tot < numBytes; addr_tot++) { // addr_tot will be 1 less than numBytes as addr starts from zero, numBytes starts from 1
    unsigned long t = millis();
    while (!(Serial.available() > 0)) { // if no data, loop until there is, or timeout
      if (millis()-t > 1000) { // check time
        Serial.println("(Ard) Timeout!");
        return false;
      }
    }
    byte datw = Serial.read(); // read byte value from serial
    Serial.write(datw); // write data back to PC to check and control data flow
    done_w = writePakByte(datw); // write value to current memory address
    if (done_w == false) {
      Serial.println("(Ard) Write byte failed!");
      break;
    }
    /*char buf[15];
    sprintf(buf, "(Ard) %04x: %02x", addr_tot, datw);
    Serial.println(buf);*/  
    if ((paged_addr == true) && (addr_low == 0xFF)) { // if using paged addressing and end of page reached, go to next page, addr_low will wrap around to zero
      nextPage();
    }
    nextAddress();
    addr_low++;
    }
  if (done_w == true) {
  Serial.println("(Ard) Write done ok");
  }
  return done_w;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void eraseBytes(word addr, word numBytes) { // eraze numBytes, starting at addr - no pages yet
  setAddress(addr);
  bool done_ok = false;
  byte addr_low = 0;
  Serial.print("(Ard) Erasing:");
  for (word i = 0; i <= numBytes; i++) { // loop through numBytes to write
    done_ok = writePakByte(0xFF); // write 0xFF
    if (done_ok == false) {
      Serial.println("(Ard) Erase failed!");
      break; // break out of for loop
    }
    if (addr_low == 0xFF) { // if end of page reached, go to next page, addr_low will wrap around to zero
      if (paged_addr == true) nextPage(); // if using paged addressing go to next page
      Serial.print("."); // "." printed for each page erased
    }
    nextAddress(); 
    addr_low++;  
  }
  if (done_ok == true) {
  Serial.println("");
  Serial.println("(Ard) Erased ok");
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void WriteMainRec() { // write record to main
  word endAddr = readAll(false); // false (0) - don't print output, just find 1st empty address
  if (readAddr(endAddr, false/* no ouput */) == 0xFF) { // move to start address & read it, if value is 0xFF write record, also prints value at address
    char main[] = "--TEST DATA"; // Main record text with leading "--" for length & identifier bytes
    byte len_main = sizeof(main)-1;// not including 0 at end
    main[0] = len_main-2; // record text length identifier byte
    main[1] = 0x90; // MAIN file identifier byte

    bool done_ok = false;
    for (byte i = 0; i < len_main; i++) { // index starts from 0, so do while i < len_str, not i <= len_str
      done_ok = writePakByte(main[i]); // write i'th char of main record
      if (((endAddr & 0xFF) == 0xFF) & (paged_addr == true)) nextPage(); // if end of page reached, and paged_addr mode is true, go to next page
      nextAddress(); // next address
      endAddr++; // increment address pointer
      if (done_ok == false) break;
    }
    if (done_ok == true) Serial.println("(Ard) add record done successfully");
    else Serial.println("(Ard) add record failed!");
  }
  else Serial.println("(Ard) no 0xFF byte to add record!");
}

// ---------------------------------------------------------------------------
// setup starts here
// ---------------------------------------------------------------------------

void setup() {
  
  //Serial.begin(9600); // fast enough?
  //Serial.begin(19200); // faster
  //Serial.begin(57600); // faster
  Serial.begin(115200); // faster
  
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

  // wait for connected datapak
  Serial.println("(Ard) Please connect Datapak, then press Enter...");
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


  Serial.println("(Ard) Select a command: e-erase, r-read pack, w-write pack, 0-print page 0, 1-print page 1, m-write test record to main");
  Serial.println("(Ard) or press x to exit");
}

// ---------------------------------------------------------------------------
// main loop starts here
// ---------------------------------------------------------------------------

void loop() { // command loop
  
  bool loop_ok = true;
  if (loop_ok && (Serial.available() > 0)) {
    char key = Serial.read();
    //char buf[15];
    //sprintf(buf, "(Ard) In: 0x%02x", byte(key)); // print input character
    //Serial.println(buf);
    
    if (key == 'e') { // erase bytes
      Serial.println("(Ard) Erase 512 bytes:"); // 512 bytes for first 2 pages
      eraseBytes(0,512); // starting from 0, erase bytes
    }
    
    if (key == 'w') { // write pack from PC data
      bool write_ok = false;
      Serial.println("(Ard) Write Serial data to pack");
      char str[] = "XXWrite"; // Check for "Write" from PC to indicate following data is write data
      if (Serial.find(str,7) == true) { // waits until timeout for Write to signal start of data
        byte numBytes[2]; // byte array to store pack image size
        byte bytes_read = Serial.readBytes(numBytes,2); // read 2 bytes for pack size, will need to be 3 if pack > 64kB !
        word numBytesTotal = (numBytes[0] << 8) + numBytes[1]; // shift 1st byte 8 bits left for high byte and add 2nd byte as low byte
        write_ok = writePakSerial(numBytesTotal); // write numBytes from file
        char buf[50];
        sprintf(buf, "(Ard) Pack size to write was: %04x bytes", numBytesTotal);
        Serial.println(buf);      
      }
      else {
        Serial.println("(Ard) No Write to begin data");
      }
      if (write_ok == false) {
        Serial.println("(Ard) Write failed!");        
      }
    }
    
    if (key == 'r') { // read pack and send to PC
      word endAddr = readAll(2); // 0 - no output, 1 - print data, 2 - dump data
      char buf[40];
      sprintf(buf, "(Ard) Size of pack is: %04x bytes", endAddr); // end address is same as size as address starts from 0, size starts from 1
      Serial.println(buf);
    }
    
    if (key == '0') { // read page 0
      Serial.println("(Ard) Page 0:");
      printPageContents(0); // print zero page - first 256 bytes of datapak
    }
    
    if (key == '1') { // read page 1
      Serial.println("(Ard) Page 1:");
      printPageContents(1); // print page 1 - second 256 bytes of datapak
    }
    
    if (key == 'm') { // add test record to main
      Serial.println("(Ard) add record to Main");
      WriteMainRec(); // add a record to main with str, need to send length when passing string in C
    }
    
    if (key == 'x') { // x to exit
      // set all control lines to input - pack will set default line states?
      pinMode(SS_N, INPUT); // deselect first
      pinMode(OE_N, INPUT);
      pinMode(PGM_N, INPUT);
      pinMode(CLK, INPUT);
      pinMode(MR, INPUT);

      ArdDataPinsToInput();
      
      Serial.println("(Ard) Please Remove Datapak");
      Serial.println("XXExit"); // send exit command to PC
      do { // endless loop
        delay(1);
      } while (true);
    }
  }
}
