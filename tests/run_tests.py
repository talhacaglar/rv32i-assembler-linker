import os
import subprocess
import glob
import json
import struct
import sys

DATA_BASE = 0x00010000

def validate_outputs(basename, obj_file, out_base):
    if basename == "uart_tx":
        expected = b"Hello PicoRV32!\n\0"
        with open(obj_file, "r", encoding="utf-8") as f:
            obj = json.load(f)
        obj_data = bytes(obj.get("data", []))
        if not obj_data.startswith(expected):
            print("  [FAIL] .string verisi beklenen newline/NUL icerigiyle uretilmedi.")
            return False

        with open(out_base + ".bin", "rb") as f:
            bin_data = f.read()
        if len(bin_data) < DATA_BASE + len(expected) or bin_data[DATA_BASE:DATA_BASE + len(expected)] != expected:
            print("  [FAIL] BIN icinde .data bolumu DATA_BASE adresine yerlestirilmedi.")
            return False

    if basename == "data_reloc":
        with open(out_base + ".bin", "rb") as f:
            bin_data = f.read()
        if len(bin_data) < DATA_BASE + 8:
            print("  [FAIL] data_reloc BIN beklenen data adresine kadar uzamadi.")
            return False
        value = struct.unpack_from("<I", bin_data, DATA_BASE)[0]
        ptr = struct.unpack_from("<I", bin_data, DATA_BASE + 4)[0]
        if value != 0x12345678 or ptr != DATA_BASE:
            print(f"  [FAIL] ABS32 data relocation hatali: value=0x{value:08x}, ptr=0x{ptr:08x}")
            return False

    return True

def run_tests():
    print("========================================")
    print(" RV32I Test Runner")
    print("========================================\n")
    
    # Derleyici yollari
    assembler = "./assembler_bin"
    linker = "./linker_bin"
    
    if not os.path.exists(assembler) or not os.path.exists(linker):
        print("[HATA] Derlenmis assembler_bin veya linker_bin bulunamadi. Lutfen 'make build' calistirin.")
        return 1

    test_files = glob.glob("tests/test_programs/*.s")
    if not test_files:
        print("[HATA] Test dosyasi bulunamadi (tests/test_programs/*.s)")
        return 1

    os.makedirs("output/tests", exist_ok=True)
    
    success_count = 0
    total = len(test_files)
    
    for asm_file in test_files:
        basename = os.path.basename(asm_file).replace('.s', '')
        obj_file = asm_file.replace('.s', '.o')
        out_base = f"output/tests/{basename}"
        
        print(f"Test: {basename}...")
        
        # Assemble
        res_asm = subprocess.run([assembler, asm_file], capture_output=True, text=True)
        if res_asm.returncode != 0:
            print(f"  [FAIL] Assembler hatasi:\n{res_asm.stderr}")
            continue
            
        # Link
        res_lnk = subprocess.run([linker, obj_file, "-o", out_base], capture_output=True, text=True)
        if res_lnk.returncode != 0:
            print(f"  [FAIL] Linker hatasi:\n{res_lnk.stderr}")
            continue

        if not validate_outputs(basename, obj_file, out_base):
            continue
            
        print(f"  [PASS] Hex: {out_base}.hex")
        success_count += 1
        
    print(f"\nSonuc: {success_count}/{total} test basarili.")
    return 0 if success_count == total else 1
    
if __name__ == '__main__':
    sys.exit(run_tests())
