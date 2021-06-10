# -*- coding: utf-8 -*-
"""
Created on Fri Apr 16 2021

@author: martin

generates .TRN file for use by OPLTRAN command, run in DOSBOX

OPLTRAN @oplfiles [without .TRN extension] [-x] [-s/o/t] 
[-x] - XP mode
[-s] - source only
[-o] - object only (default)
[-t] - source and object

e.g. OPTRAN @oplfiles -x -t

"""

import os

path = "alzan"

outfile = path + "\\oplfiles.TRN"

print("Filename is:", outfile)

with open(outfile,"w") as f_out: # open outfile for write

    files = os.listdir(path)
    out_files = []
    
    for file in files:
        n = file.find(".OPL")
        if n > 0:
            out_files.append(file[0:n])
            
        n = file.find(".opl")
        if n > 0:
            out_files.append(file[0:n])
            

    for file in out_files:
        print(file)
        f_out.write(file + "\n")