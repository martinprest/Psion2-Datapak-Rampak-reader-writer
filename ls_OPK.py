# -*- coding: utf-8 -*-
"""
OPK file reader

Created: Sept 2022

@author: martin
"""

import os


# file1 = "comms42.opk"
file1 = "rampak_colours.opk"

dat = [] #  file data 
dat_size = []

# read file data

print(f'File {file1:s}:')
# fs = os.path.getsize(f)
# print(f'File size is: {fs:d} bytes')
# fid = open(f,'rb')
with open(file1,'rb') as fid:
    fid.seek(0,2)      # go to the file end, 2-rel. to end
    fs = fid.tell()   # get the end of file location
    fid.seek(0,0)      # go back to file beginning, 0-abs
    print(f'File size: {fs:d}')        
    eof = False
    addr = 0
    # fs = 5
    while not eof:
        b = ord(fid.read(1))
        # print(addr, b)
        dat.append(b)
        addr += 1
        if addr == fs:
            eof = True
ds = len(dat)
print(f'bytes read: {ds:d} 0x{ds:06X}')
dat_size.append(ds)
    
            
dat_sz_min = min(dat_size)
# dat_sz_min = 80



# print OPK header and remove it

header = [] 
for i in range(6):
    header.append(dat[i])

print('OPK Header:',header)
size_hh = header[3]
size_h = header[4]
size_l = header[5]

size = (size_hh << 16) + (size_h << 8) + size_l # shift size_hh left 16 bits, size_h left 8 bits

print(f'size_hh: 0x{size_hh:02x} size_h: 0x{size_h:02x}, size_l: 0x{size_l:02x}, size: 0x{size:06x} {size:d}')

dat = dat[6::] # cut out OPK header bytes

# size pack

start = 0x0A # records start after ID bytes at address 10
# i = np.uint32(start) # address i is 32 bit integer
i = start

print(dat[i:i+2])

bsr = 0
sr = 0
lr = 0
end_of_pack = False
r_types = {0x80:'long',0x81:'data',0x82:'diary',0x83:'OPL',0x84:'comms',0x85:'sheet',0x86:'pager',0x87:'notepad'}
df_id = [] # list to store data file IDs

while not end_of_pack:
    if i >= 0x10000: # limit read size here, if needed
        end_of_pack = True
    if dat[i] == 0xFF: # end of pack
        skp = 0
        end_of_pack = True
    elif dat[i+1] == 0xFF: # bad short record
        skp=1
        bsr += 1
    elif dat[i] == 2 and dat[i+1] == 0x80: # long record
        skp = dat[i+2]*256 + dat[i+3] # low byte, high byte of length
        lr += 1
        print(f'long record at 0x{i:06x} length: 0x{skp-4:06x} {skp:6d} ')
    else: # short record
        skp = dat[i] + 2
        sr += 1
        r_type = dat[i+1]
        r_string = ''
        l_str = dat[i]
        if r_type == 0x81: # data file so remove 1 byte from string length for file ID (0x90 for main)
            l_str -= 1
            df_id.append(dat[i+10]) # store datafile ID
            r_type_s = f'datafile ID: {dat[i+10]:02x}'
        elif r_type >= 0x82 and r_type <= 0x87:
            try:
                r_type_s = r_types[r_type] # try looking up record type in dictionary
            except:
                r_type_s = 'unknown' # if not in dictionary - unknown
        elif r_type >= 0x90 and r_type <= 0xFE: # record from datafile with ID
            r_type_s = f'record from datafile ID: {r_type:02x}'
        else:
            r_type_s = 'unknown' # not any of the above
        for sl in range(l_str): # build record string
            r_string = r_string + chr(dat[i+2+sl])
        print(f'short record Type: 0x{r_type:02x} {r_type_s:s} at 0x{i:06x} length: 0x{skp-2:06x} : {r_string:s}  skip:{skp:4d}')
    i = i + skp

print(f'bad short records: {bsr:d}')
print(f'short records: {sr:d}')
print(f'long records: {lr:d}')
print(f'pack size: {i:06x} {i:d}')
    
# display pack data
    
data = dat
check_blank = False
      
print("\naddr   00 01 02 03 04 05 06 07   08 09 0A 0B 0C 0D 0E 0F   TEXT")
print("---------------------------------------------------------------")

file_len = size
# file_len = 256
# file_len = 512
# file_len = 1024*8

l = (file_len-1) // 16 # div (no. of complete 16's in file_len)
n = 15 - (file_len % 16) # fill in rest of 16 with zero's - just for printing
for i in range(n):
    data.append(0)
    
start = 0x00
length = 0x100
end_addr = addr + length
addr = start
ext = False
for j in range(l+1): # no. of sets 16 bytes
    out = ""
    dat2 = []
    if addr > size:
        break
    for k in range(16):
        c = data[addr]
        if check_blank == True and c != 0xFF:
            print(f'Non 0xFF byte: {c:02x} found at {addr:04x}')
            ext = True
        dat2.append(c)
        addr += 1
        if 127 < c or c < 32:
            c = 46 # .
        out = out + chr(c)
        if k == 7:
            out = out +"  "
        if addr >= end_addr:
            ext = True
    base = j*16
    print(f"{start+base:04x}   {dat2[0]:02x} {dat2[1]:02x} {dat2[2]:02x} {dat2[3]:02x} {dat2[4]:02x} {dat2[5]:02x} {dat2[6]:02x} {dat2[7]:02x}   {dat2[8]:02x} {dat2[9]:02x} {dat2[10]:02x} {dat2[11]:02x} {dat2[12]:02x} {dat2[13]:02x} {dat2[14]:02x} {dat2[15]:02x}   {out:s}")
    if ext == True:
        break
    
    