#include "linker.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Kullanim: linker <dosya1.o> [dosya2.o ...] -o <cikti> [--text-base X] [--data-base X] [--stack-top X]\n");
        return 1;
    }

    uint32_t text_base = 0x00000000;
    uint32_t data_base = 0x00010000;
    uint32_t stack_top = 0x00020000;
    char output[256] = "output";
    char obj_files[32][256];
    int obj_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            linker_copy_cstr(output, sizeof(output), argv[++i]);
        } else if (strcmp(argv[i], "--text-base") == 0 && i+1 < argc) {
            text_base = (uint32_t)strtol(argv[++i], NULL, 16);
        } else if (strcmp(argv[i], "--data-base") == 0 && i+1 < argc) {
            data_base = (uint32_t)strtol(argv[++i], NULL, 16);
        } else if (strcmp(argv[i], "--stack-top") == 0 && i+1 < argc) {
            stack_top = (uint32_t)strtol(argv[++i], NULL, 16);
        } else if (argv[i][0] != '-') {
            if (obj_count >= MAX_OBJECTS) {
                fprintf(stderr, "[ERR] Cok fazla obje dosyasi (maksimum %d)\n", MAX_OBJECTS);
                return 1;
            }
            linker_copy_cstr(obj_files[obj_count++], sizeof(obj_files[0]), argv[i]);
        }
    }

    if (obj_count == 0) {
        fprintf(stderr, "[ERR] Linklenecek obje dosyasi verilmedi.\n");
        return 1;
    }

    printf("================================================================\n");
    printf("  RV32I Linker v1.0 (PicoRV32)\n");
    printf("================================================================\n\n");

    Linker* l = (Linker*)calloc(1, sizeof(Linker));
    if (!l) {
        fprintf(stderr, "[ERR] Bellek ayrilamadi.\n");
        return 1;
    }
    linker_init(l, text_base, data_base, stack_top);

    for (int i = 0; i < obj_count; i++) {
        if (linker_load_object(l, obj_files[i]) != 0) {
            free(l);
            return 1;
        }
    }

    linker_pass1(l);
    if (linker_pass2(l) != 0) {
        free(l);
        return 1;
    }

    int failed = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s.hex", output); if (linker_write_hex(l, path) != 0) failed = 1;
    snprintf(path, sizeof(path), "%s.mem", output); if (linker_write_mem(l, path) != 0) failed = 1;
    snprintf(path, sizeof(path), "%s.bin", output); if (linker_write_bin(l, path) != 0) failed = 1;
    snprintf(path, sizeof(path), "%s.map", output); if (linker_write_map(l, path) != 0) failed = 1;

    if (failed) { free(l); return 1; }

    printf("\n[LINK] Tamamlandi!\n");
    printf("  FPGA icin: %s.mem ($readmemh ile yukle)\n", output);

    free(l);
    return 0;
}
