import random
import os
import subprocess
import time

while True:
    key = random.randbytes(16)
    key_str = ""

    for by in key:
       key_str += f"0x{int(by):02x}" + ", "

    with open("src/key.hh", "w") as keyfile:
        keyfile.write(key_str)
    
    subprocess.run(["make", "-C", "build"])
    subprocess.run(["bash", "runscript_internal.sh"])
    os.remove("out.log")
    os.remove("maps-LUT0-anchor.txt")
    os.remove("maps-LUT0-dependent.txt")
    os.remove("maps-LUT1-anchor.txt")
    os.remove("maps-LUT1-dependent.txt")
    os.remove("maps-LUT2-anchor.txt")
    os.remove("maps-LUT2-dependent.txt")
    os.remove("maps-LUT3-anchor.txt")
    os.remove("maps-LUT3-dependent.txt")
