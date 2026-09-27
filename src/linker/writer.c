#include "linker.h"

static int finish_output(FILE* f, const char* path) {
    int failed = ferror(f);
    if (fclose(f) != 0) failed = 1;
    if (failed) fprintf(stderr, "[ERR] Cikti yazilamadi: %s\n", path);
    return failed ? -1 : 0;
}

static void word_to_le(uint32_t w, uint8_t* out) {
    out[0] = (uint8_t)(w & 0xFF);
    out[1] = (uint8_t)((w >> 8) & 0xFF);
    out[2] = (uint8_t)((w >> 16) & 0xFF);
    out[3] = (uint8_t)((w >> 24) & 0xFF);
}

static void write_hex_record(FILE* f, uint8_t len, uint16_t addr, uint8_t type,
                             const uint8_t* data) {
    uint8_t chk = (uint8_t)(len + ((addr >> 8) & 0xFF) + (addr & 0xFF) + type);
    fprintf(f, ":%02X%04X%02X", len, addr, type);
    for (int i = 0; i < len; i++) {
        chk = (uint8_t)(chk + data[i]);
        fprintf(f, "%02X", data[i]);
    }
    chk = (uint8_t)(~chk + 1);
    fprintf(f, "%02X\n", chk);
}

static void write_hex_segment(FILE* f, uint32_t addr, const uint8_t* data, int size,
                              uint32_t* current_upper) {
    int pos = 0;
    while (pos < size) {
        uint32_t upper = addr >> 16;
        if (*current_upper != upper) {
            uint8_t upper_data[2] = {
                (uint8_t)((upper >> 8) & 0xFF),
                (uint8_t)(upper & 0xFF)
            };
            write_hex_record(f, 2, 0, 4, upper_data);
            *current_upper = upper;
        }

        uint32_t room_in_64k = 0x10000u - (addr & 0xFFFFu);
        int len = size - pos;
        if (len > 16) len = 16;
        if ((uint32_t)len > room_in_64k) len = (int)room_in_64k;

        write_hex_record(f, (uint8_t)len, (uint16_t)(addr & 0xFFFFu), 0, data + pos);
        pos += len;
        addr += (uint32_t)len;
    }
}

int linker_write_hex(Linker* l, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "[ERR] HEX dosyasi acilamadi: %s\n", path); return -1; }

    int text_size = l->final_text_count * 4;
    uint8_t* text_bytes = (uint8_t*)malloc(text_size > 0 ? (size_t)text_size : 1);
    if (!text_bytes) { fclose(f); return -1; }

    for (int i = 0; i < l->final_text_count; i++) {
        word_to_le(l->final_text[i], text_bytes + i * 4);
    }

    uint32_t current_upper = 0xFFFFFFFFu;
    write_hex_segment(f, l->text_base, text_bytes, text_size, &current_upper);
    if (l->final_data_size > 0)
        write_hex_segment(f, l->data_base, l->final_data, l->final_data_size, &current_upper);
    write_hex_record(f, 0, 0, 1, NULL);

    free(text_bytes);
    if (finish_output(f, path) != 0) return -1;
    printf("[LINK] HEX yazildi: %s (%d byte veri)\n",
           path, text_size + l->final_data_size);
    return 0;
}


int linker_write_mem(Linker* l, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "[ERR] MEM dosyasi acilamadi: %s\n", path); return -1; }

    fprintf(f, "// PicoRV32 BRAM Init\n");
    fprintf(f, "// TEXT: 0x%08x  DATA: 0x%08x\n\n", l->text_base, l->data_base);
    if ((l->text_base % 4) != 0 || (l->data_base % 4) != 0)
        fprintf(stderr, "[WARN] MEM yazimi word hizali olmayan base adres iceriyor.\n");

    fprintf(f, "// === TEXT SECTION ===\n");
    fprintf(f, "@%08x\n", l->text_base / 4);
    for (int i = 0; i < l->final_text_count; i++)
        fprintf(f, "%08x\n", l->final_text[i]);

    if (l->final_data_size > 0) {
        fprintf(f, "\n// === DATA SECTION (0x%08x) ===\n", l->data_base);
        fprintf(f, "@%08x\n", l->data_base / 4);
        int padded = (l->final_data_size + 3) & ~3;
        for (int i = 0; i < padded; i += 4) {
            uint32_t w = l->final_data[i]
                       | ((uint32_t)l->final_data[i+1]<<8)
                       | ((uint32_t)l->final_data[i+2]<<16)
                       | ((uint32_t)l->final_data[i+3]<<24);
            fprintf(f, "%08x\n", w);
        }
    }

    if (finish_output(f, path) != 0) return -1;
    printf("[LINK] MEM yazildi: %s\n", path);
    return 0;
}

static int write_zeros(FILE* f, uint32_t count) {
    uint8_t zeros[4096] = {0};
    while (count > 0) {
        size_t chunk = count > sizeof(zeros) ? sizeof(zeros) : count;
        if (fwrite(zeros, 1, chunk, f) != chunk) return 0;
        count -= (uint32_t)chunk;
    }
    return 1;
}

int linker_write_bin(Linker* l, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "[ERR] BIN dosyasi acilamadi: %s\n", path); return -1; }

    for (int i = 0; i < l->final_text_count; i++) {
        uint32_t w = l->final_text[i];
        uint8_t bytes[4];
        word_to_le(w, bytes);
        if (fwrite(bytes, 1, sizeof(bytes), f) != sizeof(bytes)) {
            fclose(f);
            fprintf(stderr, "[ERR] BIN yazilamadi: %s\n", path);
            return -1;
        }
    }

    uint32_t text_size = (uint32_t)l->final_text_count * 4u;
    uint32_t text_end = l->text_base + text_size;
    uint32_t gap = 0;
    if (l->final_data_size > 0 && l->data_base > text_end) {
        gap = l->data_base - text_end;
        if (!write_zeros(f, gap)) {
            fclose(f);
            fprintf(stderr, "[ERR] BIN bosluk yazilamadi: %s\n", path);
            return -1;
        }
    } else if (l->final_data_size > 0 && l->data_base < text_end) {
        fprintf(stderr, "[WARN] DATA base TEXT ile ortusuyor; BIN data bolumu ardindan yazildi.\n");
    }

    if (l->final_data_size > 0 &&
        fwrite(l->final_data, 1, (size_t)l->final_data_size, f) != (size_t)l->final_data_size) {
        fclose(f);
        fprintf(stderr, "[ERR] BIN data yazilamadi: %s\n", path);
        return -1;
    }
    if (finish_output(f, path) != 0) return -1;

    uint32_t sz = text_size + gap + (uint32_t)l->final_data_size;
    printf("[LINK] BIN yazildi: %s (%u byte)\n", path, sz);
    return 0;
}


int linker_write_map(Linker* l, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "[ERR] MAP dosyasi acilamadi: %s\n", path); return -1; }

    fprintf(f, "================================================================\n");
    fprintf(f, "  RV32I LINKER MAP - PicoRV32\n");
    fprintf(f, "================================================================\n\n");
    fprintf(f, "TEXT_BASE : 0x%08x\n", l->text_base);
    fprintf(f, "DATA_BASE : 0x%08x\n", l->data_base);
    fprintf(f, "STACK_TOP : 0x%08x\n\n", l->stack_top);

    fprintf(f, "----------------------------------------------------------------\n");
    fprintf(f, "BOLUM HARITASI\n");
    fprintf(f, "----------------------------------------------------------------\n");
    fprintf(f, "%-30s %12s %10s %12s %10s\n","Dosya","Text Basl","Text Boy","Data Basl","Data Boy");
    for (int i = 0; i < l->obj_count; i++) {
        LoadedObject* obj = &l->objects[i];
        fprintf(f, "%-30s 0x%08x %8dB  0x%08x %8dB\n",
                obj->filename, obj->text_base, obj->text_count*4,
                obj->data_base, obj->data_size);
    }
    fprintf(f, "\nToplam .text : %d byte\n", l->final_text_count*4);
    fprintf(f, "Toplam .data : %d byte\n\n", l->final_data_size);

    fprintf(f, "----------------------------------------------------------------\n");
    fprintf(f, "SEMBOL TABLOSU\n");
    fprintf(f, "----------------------------------------------------------------\n");
    for (int i = 0; i < l->sym_table.count; i++) {
        GlobalSymbol* s = &l->sym_table.syms[i];
        fprintf(f, "  %-30s 0x%08x  [%s]\n", s->name, s->address, s->src_file);
    }

    uint32_t entry = sym_resolve(&l->sym_table, "_start");
    fprintf(f, "\n----------------------------------------------------------------\n");
    fprintf(f, "GIRIS NOKTASI\n");
    fprintf(f, "----------------------------------------------------------------\n");
    fprintf(f, "  _start = 0x%08x\n", entry != 0xFFFFFFFF ? entry : l->text_base);

    if (finish_output(f, path) != 0) return -1;
    printf("[LINK] MAP yazildi: %s\n", path);
    return 0;
}
