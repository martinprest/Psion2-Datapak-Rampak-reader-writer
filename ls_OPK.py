# -*- coding: utf-8 -*-
"""
OPK file reader

Created: Sept 2022

@author: martin
"""

# import os

# file1 = "comms42.opk"
# file1 = "rampak_colours.opk"
file1 = "testpak.opk"
# file1 = "test.opk"

dat = [] #  file data 

# read file data

print(f'File {file1:s}:')
# fs = os.path.getsize(f)
# print(f'File size is: {fs:d} bytes')
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
print(f'bytes read: {ds:d} 0x{ds:06x}')

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

# size & list pack

start = 0x0A # records start after ID bytes at address 10
# i = np.uint32(start) # address i is 32 bit integer
i = start

print('Record list:')

bsr = 0 # bad short record count
sr = 0 # short record count
lr = 0 # long record count
end_of_pack = False
r_types = {0x80:'long',0x81:'data',0x82:'diary',0x83:'OPL',0x84:'comms',0x85:'sheet',0x86:'pager',0x87:'notes'}
df_id = [] # list to store data file IDs
df_name = [] # list to store data file names

while not end_of_pack:
    if i >= 0x10000: # limit read size here, if needed
        end_of_pack = True
    rec_len = dat[i] # 1st byte is record length
    if rec_len == 0xFF: # end of pack
        skp = 0
        end_of_pack = True
        print(f'0x{i:04x} end of pack')
        break
    rec_type = dat[i+1] # 2nd byte is record type (only 2 main types: short or long)
    r_type = rec_type | 0x80 # OR with 0x80, as could be deleted
    if rec_type == 0xFF: # bad short record
        skp = 2 # ignore length if bad, skip past length and type bytes
        bsr += 1
        print(f'0x{i:04x} bad short record')
    elif rec_type == 0x80: # long record - preceding short record has: deleted?, type, filename
        skp = (dat[i+2] << 8) + dat[i+3] + 4 # high byte, low byte of long rec length
        lr += 1
        print(f'0x{i:04x} Long  Length: 0x{skp-4:04x} skip :0x{skp:04x}')
    else: # short record
        skp = rec_len + 2
        sr += 1
        r_string = ''
        for sl in range(rec_len): # build record string
            c = dat[i+2+sl]
            r_string += chr(c)
        if  rec_type >> 7: # shift right 7 bits to see if MSB is high - not deleted
            r_del = 'n' # not deleted
        else:
            r_del = 'y' # deleted
        if r_type == 0x81: # datafile so read name and file ID (0x90 for main)
            r_string = r_string[0:8] # file names are always 8 chars (range end value is exclusive)
            df_id.append(dat[i+10]) # store datafile ID
            df_name.append(r_string.strip()) # store datafile name & strip spaces
            r_type_s = f'datafile ID: {dat[i+10]:02x}'
        elif r_type >= 0x82 and r_type <= 0x8F:
            try:
                r_type_s = r_types[r_type] # try looking up record type in dictionary
            except:
                r_type_s = 'unknown' # if not in dictionary - unknown
        elif r_type >= 0x90 and r_type <= 0xFE: # record from datafile with ID
            try:
                p = df_id.index(r_type) # look for datafile ID in list
                dfn = df_name[p] # get name from list
            except:
                dfn = 'ID not found'
            r_type_s = f'record from datafile ID: {r_type:02x} {dfn:s}'
        else:
            r_type_s = 'unknown' # not any of the above
        print(f'0x{i:04x} Short Length: 0x{skp-2:04x} skip: 0x{skp:04x} deleted?: {r_del:s} Type: 0x{dat[i+1]:02x} {r_type_s:s} : {r_string:s}')
    i = i + skp

print(f'bad short records: {bsr:d}')
print(f'short records: {sr:d}')
print(f'long records: {lr:d}')
print(f'pack size: 0x{i:04x} {i:d}')

if i == size:
    print('Sizing matches OPK size!')
else:
    print('Sizing does not match OPK size!!')
    
# display pack data
    
data = dat
check_blank = False
      
print("\naddr   00 01 02 03 04 05 06 07   08 09 0A 0B 0C 0D 0E 0F   TEXT")
print("---------------------------------------------------------------")

# size = 0x200

l = (size-1) // 16 # div (no. of complete 16's in size)
n = 15 - (size % 16) # fill in rest of 16 with zero's - just for printing
for i in range(n):
    data.append(0)
    
start = 0x00
end_addr = start + size
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
    
    