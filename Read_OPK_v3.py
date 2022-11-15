# -*- coding: utf-8 -*-
"""
OPK file reader

Created: March 2021

@author: martin
"""

import os

# file = "rampak_colours.opk"
# file = "comms42.opk"
# file = "linear_datapak.opk"
file = "testpak.opk"

fid = open(file,'rb')
f_in_size = os.path.getsize(file)
print(f'File size is: 0x{f_in_size:06x} bytes')

opk_pack = True
#opk_pack = False

read_fixed_size = False
# read_fixed_size = True
read_pack_size = 0x7e9b

# check_blank = True # checks for a blank datapak
check_blank = False

header = []

if opk_pack == True:
    for i in range(6):
        header.append(ord(fid.read(1)))
    
    print('OPK Header:',header)
    size_hh = header[3]
    size_h = header[4]
    size_l = header[5]
    
    size = (size_hh << 16) + (size_h << 8) + size_l # shift size_hh left 16 bits, size_h left 8 bits
    
    print(f'size_hh: 0x{size_hh:02x} size_h: 0x{size_h:02x}, size_l: 0x{size_l:02x}, size: 0x{size:06x}')

eof = False
end_chk = 0
numFFchk = 2
data=[]
addr = 0
while eof != True:
    dat = fid.read(1)
    if dat > bytes(0):
        n = ord(dat)
    else:
        n = 0
    if 31 < n < 127:
        n2 = n
    else:
        n2 = 46
#    print(f'{addr:03x}: {n:02x} {chr(n2):s}')
    data.append(n)
    if read_fixed_size == True:
        if addr >= read_pack_size:
            break
    elif addr >= size:
        break
    addr += 1
fid.close()
# file_len = addr
# file_len = 256
# file_len = 512
# file_len = 1024*8
file_len = size

c = data[0]
ID = c
print(f"\nByte #0: Hex: {c:02x}  Bin: {c:08b}")
print("ID Byte")


ID_dict = {0x01:{-1: 'Mk II pack?',
                 0: 'yes - Valid Mk II pack',
                 1: 'no - Not a valid Mk II pack'},
           0x02:{-1: 'Type?',
                 0: 'Ram pack',
                 1: 'EPROM'},
           0x04:{-1: 'Addressing?',
                 0: 'linear',
                 1: 'paged'},
           0x08:{-1: 'Write protcted?',
                 0: 'yes (flashpaks are write protected',
                 1: 'no'},
           0x10:{-1: 'Bootable?',
                 0: 'yes',
                 1: 'no'},
           0x20:{-1: 'Copyable?',
                 0: 'no',
                 1: 'yes'},
           0x40:{-1: 'Normally set',
                 0: 'Flashpak or Trap Rampak',
                 1: 'Set'},
           0x80:{-1:'Mk1 Pack?',
                 0: 'no',
                 1: 'yes'}}

bit = 1
bit_val = 1
while bit <= 8:
    if (c & bit_val) == 0:
        val = 0
    else:
        val = 1
    print(f'Bit: {bit}, 0x{bit_val:02x} {ID_dict[bit_val][-1]:<20} {val:<4} {ID_dict[bit_val][val]:<20}\n')
    bit += 1
    bit_val = bit_val << 1 # rotate left 1 bit

# byte 1 SZ
c = data[1]
size = c*8
SZ = c
print(f"\nByte #1: Hex: {c:02x}  Bin: {c:08b}")
print(f"Size of pack is {size:d} kB")


print("\nTime stamp:")
# byte 1 YR
c = data[2]
print(f"\nByte #2: Hex: {c:02x}  Bin: {c:08b}")
print(f"Year at time of sizing was {c+1900:d}")
YR = c

# byte 3 MTH
c = data[3]
print(f"\nByte #3: Hex: {c:02x}  Bin: {c:08b}")
print("Month at time of sizing was {:d}".format(c+1))
MTH = c

# byte 4 DAY
c = data[4]
print(f"\nByte #4: Hex: {c:02x}  Bin: {c:08b}")
print("Day at time of sizing was {:d}".format(c+1))
DAY = c

# byte 5 HR
c = data[5]
print(f"\nByte #5: Hex: {c:02x}  Bin: {c:08b}")
print("Hour at time of sizing was {:d}".format(c))
HR = c

# byte 6 FRH
c = data[6]
print(f"\nByte #6: Hex: {c:02x}  Bin: {c:08b}")
print(f"Free Running Counter (High) at time of sizing was {c:d}")
FRH = c

# byte 7 FRL
c = data[7]
print(f"\nByte #7: Hex: {c:02x}  Bin: {c:08b}")
print(f"Free Running Counter (Low) at time of sizing was {c:d}")
FRL = c

FRCT = (FRH<<8) + FRL
print(f"\nFree Running Counter (Total) at time of sizing was {FRCT:d}")

print("\nChecksum:")

# byte 8 CHKH
c = data[8]
print(f"\nByte #8: Hex: {c:02x}  Bin: {c:08b}")
print(f"Checksum (High) at time of sizing was 0x{c:02x}")
CHKH = c

# byte 9 CHKL
c = data[9]
print(f"\nByte #9: Hex: {c:02x}  Bin: {c:08b}")
print(f"Checksum (Low) at time of sizing was 0x{c:02x}")
CHKL = c

CHKT = (CHKH<<8) + CHKL
print(f"\nChecksum (Total) at time of sizing was {CHKT:d} 0x{CHKT:04x}")

CHKSUM = (ID<<8) + SZ + (YR<<8) + MTH + (DAY<<8) + HR + (FRH<<8) + FRL

chk_h = CHKSUM & 0xFF00 # mask for high byte
chk_l = CHKSUM & 0xFF # mask for low byte
print(f"\nchk_l = 0x{chk_l:04x}")
print(f"chk_h = 0x{chk_h:04x}")
CHKSUM = CHKSUM & 0xFFFF # mask for 16 bits
print(f"Checksum (calculated from bytes) is {CHKSUM:d} 0x{CHKSUM:04x}")

print("\nMain data file ID usually follows:")
print("Byte #0A: 09 - length 9 chars")
print("Byte #0B: 81 - data filename record")
print("Bytes #0C to 13: MAIN followed by 4 spaces, padding to 8 chars")
print("Byte #14: 90 - Main file identifier")
      
print("\naddr   00 01 02 03 04 05 06 07   08 09 0A 0B 0C 0D 0E 0F   TEXT")
print("---------------------------------------------------------------")


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
        c = data[addr]
        if check_blank == True and c != 0xFF:
            print(f'Non 0xFF byte: {c:02x} found at {addr:04x}')
            ext = True
        dat2.append(c)
        addr += 1
        if 127 < c or c < 0x20:
            c = 0x2e
        out = out + chr(c)
        if k == 7:
            out = out +"  "
    base = j*16
    print(f"{base:04x}   {dat2[0]:02x} {dat2[1]:02x} {dat2[2]:02x} {dat2[3]:02x} {dat2[4]:02x} {dat2[5]:02x} {dat2[6]:02x} {dat2[7]:02x}   {dat2[8]:02x} {dat2[9]:02x} {dat2[10]:02x} {dat2[11]:02x} {dat2[12]:02x} {dat2[13]:02x} {dat2[14]:02x} {dat2[15]:02x}   {out:s}")
    if ext == True:
        break