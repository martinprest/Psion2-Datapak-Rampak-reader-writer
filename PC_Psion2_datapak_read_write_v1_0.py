# -*- coding: utf-8 -*-
"""
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
works with: Arduino_Psion2_datapak_read-write_v1_0.ino on Arduino

"""

import keyboard as kb
import serial
import time
import os

#BaudRate = 9600
#BaudRate = 19200
#BaudRate = 57600
BaudRate = 115200

SerialPort = 'COM3'

numFFchk = 3 # must be same as Arduino program

set_Rampak_ID = True # modifies ID byte to set pack as a rampak

fix_pack_size = False
#fix_pack_size = True
#pack_size_out = 1 # 1*8 = 8 kB
pack_size_out = 2 # 2*8 = 16 kB
#pack_size_out = 4 # 4*8 = 32 kB

# file for data write to a pack
infile = "rampak_colours.opk"
print("Input filename:",infile)
f_size = os.path.getsize(infile)
print(f'(PC) File size (decimal) is: {f_size} bytes')

# file for data read from a pack
outfile = "datapak_test.opk"
print("Output filename:",outfile)
f_out_open = False

def WritePak():
    print("(PC) Write")
    f_in = open(infile,'rb')
    data = f_in.read(3) # Should be "OPK"
    header = data.decode()
    print(f'(PC) OPK header: "{header:s}"')
    if header != "OPK":
        print("Error! Not an OPK file!")
        return
    size_hh = ord(f_in.read(1))
    size_h = ord(f_in.read(1))
    size_l = ord(f_in.read(1))
    
    f_in_size = (size_hh << 16) + (size_h << 8) + size_l # shift size_h left 16 bits, size_h left 8 bits
    print(f'(PC) Pack image size: size_hh: 0x{size_hh:02x}, size_h: 0x{size_h:02x}, size_l: 0x{size_l:02x}, size: 0x{f_in_size:06x}')
    
    if f_in_size > 0xFFFF:
        print("(PC) File too big to write!")
        return
        
    size_h = (f_in_size & 0xFF00) >> 8 # high byte, shift right 8 bits, top byte (size_hh) removed, so max size is 64k!!
    size_l = f_in_size & 0xFF # lowest 8 bits, AND with 0xFF
    
    time.sleep(0.2) # 0.2 second delay for Arduino to send messages
    while ser.inWaiting(): # read & print lines from Arduino until none left
        line_in = ser.readline() 
        print("(PC) Empty buffer:", line_in.decode(), end='') # decode from bytes
    
    ser.write("XXWrite".encode()) # encode to bytes - tells Arduino that following bytes are for write to datapak
    ser.write(bytes([size_h])) # send high byte
    ser.write(bytes([size_l])) # send low byte
    
    print(f'(PC) size_h: 0x{size_h:02x}, size_l: 0x{size_l:02x}')
    
    read_file = True
    addr = 0
    data = []
    
    while read_file: # read file data to write to datapak
        dat_out = f_in.read(1) # read from file - initially must be first byte after header
        n = ord(dat_out)
        
        if addr <= 9: # first 10 bytes (0-9) stored for checksum
            
            # bytes to be modified:
            if addr == 0:
                if set_Rampak_ID == True:            
                    n = n & 0b11111101 # clear bit 1 - rampak
                    n = n | 0b100 # set bit 2 - Paged
                print(f'0: ID byte: {n:02x}')
            if addr == 1:
                if fix_pack_size == True:
                    print(f'1:Pack size was: {n*8:d} kB, now is: {pack_size_out*8:d} kB')
                    n = pack_size_out
                else:
                    print(f'1: Pack size is: {n*8:d} kB')
     
            # non-modified bytes:            
            if addr == 2:
                print(f'Timestamp:')
                print(f'2: Year: {n+1900:d}') # +1900 for year
            if addr == 3:
                print(f'3: Month: {n+1:d}') # +1 for month
            if addr == 4:
                print(f'4: Day: {n+1:d}')# +1 for day
            if addr == 5:
                print(f'5: Hour: {n:d}') # hour is not +1
            if addr == 6:
                print(f'Free running counter at timestamp:')
                FRH = n
                print(f'6: High byte: 0x{FRH:02x}')
            if addr == 7:
                FRL = n
                print(f'7: Low byte: 0x{FRL:02x}')
                FRCT = (FRH<<8) + FRL                
                print(f'Free running counter (Total): {FRCT:d} 0x{FRCT:04x}')
                
            data.append(n) # store bytes for checksum, must include modified bytes
                  
            # checksum bytes:            
            if addr == 8:
                print(f'Checksum:')
                CHKSUM = (data[0]<<8) + data[1] + (data[2]<<8) + data[3] + (data[4]<<8) + data[5] + (data[6]<<8) + data[7]
                chk_h = (CHKSUM & 0xFF00) >> 8 # mask for high byte
                chk_l = CHKSUM & 0xFF # mask for low byte            
                print(f'8: chk_h = 0x{chk_h:02x}')
                print(f'9: chk_l = 0x{chk_l:02x}')
                CHKSUM = CHKSUM & 0xFFFF # mask for 16 bits
                print(f'Checksum (calculated) is: {CHKSUM:d} 0x{CHKSUM:04x}')
                n = chk_h
            if addr == 9:
                n = chk_l
            
        dat_out = bytes([n])
        ser.write(dat_out)
        t = time.time()
        while not (ser.inWaiting() > 0): # If no data, wait and check for timeout
            if time.time()-t > 1:
                read_file = False
                print("(PC) Timeout!")
        dat_in = ser.read(1) # read check byte back from Arduino
        n = ord(dat_in) # convert to number
        if 31 < n < 127: # printable character
            n2 = n
        else: # replace non-printable character
            n2 = 46 # character "."
        if addr >= 8:
            print(f'{addr:04x} {n:02x} {chr(n2):s}  ', end='')
        if dat_in != dat_out:
            print("(PC) Write data not verified by Arduino!")
            read_file = False
        addr += 1
        if addr >8 and addr % 0x08 == 0: # if remainder of addr div 8 is zero, newline
            print("") # newline
        if addr >= f_in_size:
            read_file = False
            print("") # newline at end of file
    f_in.close()
    
def ReadPak():
    with open(outfile,'wb') as f_out: # open file for output
        f_out.write("OPK".encode())
        f_out.write(bytes(3)) # write 3 zero bytes for size, written later
        write_file = True
        end_chk = False
        addr = 0
        while write_file == True:
            if ser.inWaiting():
                dat = ser.read(1) # read 1 value
                ser.write(dat) # echo back to Arduino for verify
                n = ord(dat) # convert char or b'\xff' hex byte to value
                if 31 < n < 127:
                    n2 = n
                else:
                    n2 = 46 # character "." for non-printable character
                print(f'{addr:04x} {n:02x} {chr(n2):s}  ', end='')
                f_out.write(dat) # write it to file
                if n == 0xFF: 
                    end_chk += 1                
                else: end_chk = 0
                if end_chk >= numFFchk: # must be same as Arduino program
                    write_file = False
                    break  
                addr += 1
                if addr % 0x08 == 0: # if remainder of addr div 8 is zero, newline
                    print("") # newline
        f_out.seek(3) # move back to size bytes in PC outfile, byte 3: 0, 1, 2, 3
        addr -= (numFFchk - 1) # decrement addr pointer to final 0xFF byte so addr is number of bytes in pack
        addr_hh = (addr & 0xFF0000) >> 16 # high byte, mask & shift right 16 bits
        addr_h = (addr & 0xFF00) >> 8 # middle byte, mask & shift right 8 bits
        addr_l = addr & 0xFF # low byte
        f_out.write(bytes([addr_hh,addr_h,addr_l])) # write size, includes 0xFF bytes at end
        print("") # newline  
        print("(PC) Datapak read to file has ended")

loop = True
inp = ''

try:
    with serial.Serial(SerialPort, BaudRate, timeout=2) as ser:
        print("Reading:",ser.name)
        while loop:
            time.sleep(0.001) # 1 ms delay to slow loop down
                    
            if ser.inWaiting(): # message from Arduino waiting in serial input buffer
                msg = ser.readline()
                msg_d = msg.decode('utf-8','ignore') # decode from utf-8
        #        if ord(msg_d) == 10: print() # newline
                msg_s = "".join(i for i in msg_d if 32 <= ord(i) <= 126) # remove unwanted characters
        #        print(msg_s,end='') # message from Arduino, supress newline
                print(msg_s) # message from Arduino
                if msg_s == "XXRead": 
                    ReadPak()
                if msg_s == "XXExit": 
                    loop = False
                    
            else: # no message from Arduino, so check for PC key press
                    if kb.is_pressed('x'):              
                        inp='x'
                    if kb.is_pressed('0'):          
                        inp='0'
                    if kb.is_pressed('1'): 
                        inp='1'
                    if kb.is_pressed('r'): 
                        inp='r'
                    if kb.is_pressed('w'): 
                        inp='w'
                    if kb.is_pressed('m'): 
                        inp='m'
                    if kb.is_pressed('e'): 
                        inp='e'
                    if kb.is_pressed('Enter'):
                        inp = '\n'
                    if inp != '':
                        while kb.is_pressed(inp): # wait until key not pressed any more
                            pass # do nothing
#                        print(f'(PC) Key pressed: {inp:s} as bytes:',inp.encode()) # print keypress
                        ser.write(inp.encode()) # write inp key to serial
                        if inp == 'w':
                            WritePak()
                        inp = ''
except:
    print("\nError! Maybe Arduino not connected to serial port?")
        