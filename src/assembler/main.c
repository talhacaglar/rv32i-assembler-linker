#include "assembler.h"
#include "parser.h"
#include "../common/utils.h"

void asm_init(Assembler* a, const char* filename) {
    memset(a, 0, sizeof(Assembler));
    copy_cstr(a->obj.filename, sizeof(a->obj.filename), filename);
    copy_cstr(a->current_section, sizeof(a->current_section), "text");
}

int asm_assemble_file(Assembler* a, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "[ERR] Kaynak dosya acilamadi: %s\n", path); return -1; }

    /* Satırları oku */
    char lines[4096][MAX_LINE_LEN];
    int lc = 0;
    while (lc < 4096 && fgets(lines[lc], MAX_LINE_LEN, f)) lc++;
    if (!feof(f)) {
        fprintf(stderr, "[ERR] Kaynak dosya cok uzun (maksimum 4096 satir): %s\n", path);
        fclose(f);
        return -1;
    }
    fclose(f);

    printf("[ASM] Islem: %s (%d satir)\n", path, lc);

    first_pass(a, lines, lc);
    second_pass(a, lines, lc);
    if (a->error_count) return -1;
    resolve_local_relocs(a);

    printf("  .text : %d byte (%d komut)\n", a->obj.text_count*4, a->obj.text_count);
    printf("  .data : %d byte\n", a->obj.data_size);
    printf("  Sembol: %d adet | Reloc: %d adet\n", a->obj.sym_count, a->obj.reloc_count);

    return 0;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Kullanim: assembler <dosya1.s> [dosya2.s ...]\n");
        return 1;
    }

    printf("========================================\n");
    printf("  RV32I Assembler v1.0 (PicoRV32)\n");
    printf("========================================\n\n");

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        Assembler asm_state;
        asm_init(&asm_state, argv[i]);

        if (asm_assemble_file(&asm_state, argv[i]) != 0) { failed = 1; continue; }

        /* global bayraklarini sembollere yansit */
        for (int j = 0; j < asm_state.obj.sym_count; j++) {
            for (int k = 0; k < asm_state.obj.global_count; k++) {
                if (strcmp(asm_state.obj.symbols[j].name, asm_state.obj.globals[k]) == 0)
                    asm_state.obj.symbols[j].is_global = 1;
            }
        }

        /* .o dosyasini yaz */
        char out_path[300];
        copy_cstr(out_path, sizeof(out_path), argv[i]);
        char* dot = strrchr(out_path, '.');
        if (dot) {
            copy_cstr(dot, sizeof(out_path) - (size_t)(dot - out_path), ".o");
        } else if (strlen(out_path) + 2 < sizeof(out_path)) {
            strcat(out_path, ".o");
        } else {
            fprintf(stderr, "[ERR] Cikti dosya yolu cok uzun: %s\n", argv[i]);
            failed = 1;
            continue;
        }

        if (obj_save_json(&asm_state.obj, out_path) != 0) { failed = 1; continue; }
        printf("  -> %s\n\n", out_path);
    }

    printf("Assembler tamamlandi.\n");
    return failed ? 1 : 0;
}
