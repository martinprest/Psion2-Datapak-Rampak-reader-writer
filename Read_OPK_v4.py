# -*- coding: utf-8 -*-
"""
OPK file reader

Created: March 2021

@author: martin
"""

import os

# file = "rampak_colours.opk"
file = "comms42.opk"
# file = "linear_datapak.opk"
# file = "testpak.opk"

print(f'File: {file:s}')
fid = open(file,'rb')
f_in_size = os.path.getsize(file)
print(f'File size is: 0x{f_in_size:06x} bytes')

# check_blank = True # checks for a blank datapak
check_blank = False

header = []

for i in range(6):
    header.append(ord(fid.read(1)))

print('OPK Header:',header)
size_hh = header[3]
size_h = header[4]
size_l = header[5]

size = (size_hh << 16) + (size_h << 8) + size_l # shift size_hh left 16 bits, size_h left 8 bits

print(f'size_hh: 0x{size_hh:02x} size_h: 0x{size_h:02x}, size_l: 0x{size_l:02x}, size: 0x{size:06x}\n')

data=[]
addr = 0
while True:
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
    if addr >= size:
        break
    addr += 1
fid.close()

print('ID Byte:')
n = data[0]
print(f'Byte #0: Hex: 0x{n:02x}  Bin: {n:08b}')

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
    if (n & bit_val) == 0:
        val = 0
    else:
        val = 1
    print(f'Bit: {bit}, 0x{bit_val:02x} {ID_dict[bit_val][-1]:<20} {val:<4} {ID_dict[bit_val][val]:<20}')
    bit += 1
    bit_val = bit_val << 1 # rotate left 1 bit
print('')

print(f'Size of pack is {data[1]*8:d} kB\n')

if (data[0] & 0x10) != 0x10: # bootable pack
    bootable_header = {2:{'txt':'code type',0:'software',1:'hardware'},
                       3:{'txt':'id',0xc0:'RS232',0xbf:'bar code reader',0xbe:'swipe card reader',0x0a:'concise oxford spelling checker'},
                       4:{'txt':'version (binary coded decimal: 0xnm for version n.m'},
                       5:{'txt':'priority (can be same as id)'},
                       6:{'txt':'boot code pack address (high)'},
                       7:{'txt':'boot code pack address (low)'}}
    for i in range(6):
        try:
            print(f'{bootable_header[i+2]["txt"]}: {bootable_header[i+2][data[i+2]]}')
        except:
            print(f'{bootable_header[i+2]["txt"]}: 0x{data[i+2]:02x}')
    print(f'boot code pack address: 0x{data[6]:02x}{data[7]:02x}')
                   
else: # standard pack
    standard_header = {2:['Year at time of sizing was {:d}','n+1900'],
                       3:['Month at time of sizing was {:d}','n+1'],
                       4:['Day at time of sizing was {:d}','n+1'],
                       5:['Hour at time of sizing was {:d}','n'],
                       6:['Free Running Counter (High) at time of sizing was 0x{:02x}','n'],
                       7:['Free Running Counter (Low) at time of sizing was 0x{:02x}','n']}
    for i in range(6):
        n = data[i+2]
        print(standard_header[i+2][0].format(eval(standard_header[i+2][1])))

    FRH = data[6]
    FRL = data[7]
    FRCT = (FRH<<8) + FRL
    print(f"Free Running Counter (Total) at time of sizing was {FRCT:d} 0x{FRCT:04x}")

print(f'\nChecksum (High) at time of sizing was 0x{data[8]:02x}')
print(f'Checksum (Low) at time of sizing was 0x{data[9]:02x}')
CHKH = data[8]
CHKL = data[9]
CHKT = (CHKH<<8) + CHKL
print(f'Checksum (Total) at time of sizing was {CHKT:d} 0x{CHKT:04x}')

CHKSUM = (data[0]<<8) + data[1] + (data[2]<<8) + data[3] + (data[4]<<8) + data[5]+ (data[6]<<8) + data[7]

chk_h = CHKSUM & 0xFF00 # mask for high byte
chk_l = CHKSUM & 0xFF # mask for low byte
print(f'chk_h = 0x{chk_h:04x}')
print(f'chk_l = 0x{chk_l:04x}')
CHKSUM = CHKSUM & 0xFFFF # mask for 16 bits
print(f'Checksum (calculated from bytes) is {CHKSUM:d} 0x{CHKSUM:04x}')

print('\nMain data file ID usually follows:')
print('Byte #0A: 09 - length 9 chars')
print('Byte #0B: 81 - data filename record')
print('Bytes #0C to 13: MAIN followed by 4 spaces, padding to 8 chars')
print('Byte #14: 90 - Main file identifier')
      
print('\naddr   00 01 02 03 04 05 06 07   08 09 0A 0B 0C 0D 0E 0F   TEXT')
print('---------------------------------------------------------------')

# size = addr
size = 256
# size = 512
# size = 1024*8

l = (size-1) // 16 # div (no. of complete 16's in size)
n = 15 - (size % 16) # fill in rest of 16 with zero's - just for printing
for i in range(n):
    data.append(0)
    
addr = 0
ext = False
for j in range(l+1): # no. of sets 16 bytes
    out = ''
    dat2 = []
    for k in range(16):
        n = data[addr]
        if check_blank == True and n != 0xFF:
            print(f'Non 0xFF byte: {n:02x} found at {addr:04x}')
            ext = True
        dat2.append(n)
        addr += 1
        if 127 < n or n < 0x20:
            n = 0x2e
        out = out + chr(n)
        if k == 7:
            out = out +'  '
    base = j*16
    print(f'{base:04x}   {dat2[0]:02x} {dat2[1]:02x} {dat2[2]:02x} {dat2[3]:02x} {dat2[4]:02x} {dat2[5]:02x} {dat2[6]:02x} {dat2[7]:02x}   {dat2[8]:02x} {dat2[9]:02x} {dat2[10]:02x} {dat2[11]:02x} {dat2[12]:02x} {dat2[13]:02x} {dat2[14]:02x} {dat2[15]:02x}   {out:s}')
    if ext == True:
        break