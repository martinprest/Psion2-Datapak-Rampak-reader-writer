# -*- coding: utf-8 -*-
"""
OPK file reader

Created: March 2021

@author: martin
"""

import os
import sys

file1 = "comms42.opk"
# file2 = "comms42_test_7e9b.opk"
file2 = "comms_linear_test.opk"

files = [file1,file2]

dat = [] # wil be a list of file data in lists
dat_size = []

# read file data

for i,f in enumerate(files):
    print(f'File {i+1:d}:')
    dat.append([]) # add list to list of lists
    # fs = os.path.getsize(f)
    # print(f'File size is: {fs:d} bytes')
    # fid = open(f,'rb')
    with open(f,'rb') as fid:
        fid.seek(0,2)      # go to the file end, 2-rel. to end
        fs = fid.tell()   # get the end of file location
        fid.seek(0,0)      # go back to file beginning, 0-abs
        print(f'File size: {fs:d}')        
        eof = False
        addr = 0
        # fs = 5
        while not eof:
            b = fid.read(1)
            # print(addr, b)
            dat[i].append(b)
            addr += 1
            if addr == fs:
                eof = True
    d = dat[i]
    ds = len(d)
    print(f'bytes read: {ds:d} 0x{ds:06X}')
    dat_size.append(ds)
    
# compare file data
            
dat_sz_min = min(dat_size)
# dat_sz_min = 80
        
# add = []
dd = [0,0]
dc = [0,0]
start = 0
for addr in range(0,dat_sz_min):
    # add.append(addr)
    for i in range(2):
        d = dat[i][addr]
        dd[i] = ord(d)
        dc[i] = dd[i]
        if 127 < dd[i] or dd[i] < 32:
                    dc[i] = 46 # '.' make char a dot, if not printable
        dc[i] = chr(dc[i])
    
    if dd[0] != dd[1] and addr > start:
        print(f'files differ at addr:{addr:04x} File1:{dd[0]:02x} {dc[0]:s} File2:{dd[1]:02x} {dc[1]:s}')
        diff = True


# # sys.exit()

header = [] 
for i in range(6):
    header.append(ord(dat[0][i]))

print('OPK Header:',header)
size_hh = header[3]
size_h = header[4]
size_l = header[5]

size = (size_hh << 16) + (size_h << 8) + size_l # shift size_hh left 16 bits, size_h left 8 bits

print(f'size_hh: 0x{size_hh:02x} size_h: 0x{size_h:02x}, size_l: 0x{size_l:02x}, size: 0x{size:06x}')


data = dat[1]
check_blank = False

      
print("\naddr   00 01 02 03 04 05 06 07   08 09 0A 0B 0C 0D 0E 0F   TEXT")
print("---------------------------------------------------------------")

file_len = 256
# file_len = 512
# file_len = 1024*8

l = (file_len-1) // 16 # div (no. of complete 16's in file_len)
n = 15 - (file_len % 16) # fill in rest of 16 with zero's - just for printing
for i in range(n):
    data.append(0)
    
addr = 0
ext = False
for j in range(l+1): # no. of sets 16 bytes
    out = ""
    dat2 = []
    for k in range(16):
        c = ord(data[addr])
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
    base = j*16
    print(f"{base:04x}   {dat2[0]:02x} {dat2[1]:02x} {dat2[2]:02x} {dat2[3]:02x} {dat2[4]:02x} {dat2[5]:02x} {dat2[6]:02x} {dat2[7]:02x}   {dat2[8]:02x} {dat2[9]:02x} {dat2[10]:02x} {dat2[11]:02x} {dat2[12]:02x} {dat2[13]:02x} {dat2[14]:02x} {dat2[15]:02x}   {out:s}")
    if ext == True:
        break