# -*- coding: utf-8 -*-
"""
Created on Fri Apr 16 2021

@author: martin

generates .BLD file for use by BLDPACK command, run in DOSBOX

BLDPACK bldfile [without .BLD extension]

e.g. BLDPACK gamepak

"""

import os

paths = ("alzan","invader2") # paths with OB3 files for pack

outfile = "gamepak.BLD"
packname = "GAMES"

#size = "16"
#nocopy = "NOCOPY"
#nowrite = "NOWRITE"

size = ""
nocopy = ""
nowrite = ""

print("Filename is:", outfile)

pack = packname + " " + size + " " + nocopy + " " + nowrite + "\n" # add size, nocopy or nowrite here, separated by spaces, if needed
print(pack,end='')

with open(outfile,"w") as f_out: # open outfile for write
    f_out.write(pack) 

for path in paths:
    files = os.listdir(path)
    out_files = []
    
    for file in files:
        n = file.find(".OB3")
        if n > 0:
            out_files.append(file[0:n])
            
    with open(outfile,"a") as f_out: # open outfile for append
        for file in out_files:
            line = path + "\\" + file + " OB3 \n"
            print(line, end='')
            f_out.write(line)