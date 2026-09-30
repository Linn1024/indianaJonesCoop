"""Print a CPU address interval from an emulator's mapped $8000-$FFFF dump."""
import sys
from pathlib import Path
from py65.devices.mpu6502 import MPU
from py65.disassembler import Disassembler

mpu = MPU()
mpu.memory[0x8000:] = Path(sys.argv[1]).read_bytes()
disassembler = Disassembler(mpu)
pc, end = int(sys.argv[2], 16), int(sys.argv[3], 16)
while pc < end:
    length, instruction = disassembler.instruction_at(pc)
    print(f"{pc:04X} {instruction}")
    pc += length
