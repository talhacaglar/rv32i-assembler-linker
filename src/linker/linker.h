#ifndef LINKER_H
#define LINKER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OBJECTS      32
#define MAX_GLOBAL_SYMS  2048
#define MAX_TOTAL_TEXT   131072   /* 512KB word */
#define MAX_TOTAL_DATA   131072   /* 128KB byte */
#define MAX_NAME_LEN     128
#define MAX_RELOCS       4096

/* ============================================================
   LINKER SEMBOL TABLOSU
   ============================================================ */
typedef struct {
    char     name[MAX_NAME_LEN];
    uint32_t address;      /* mutlak adres */
    char     src_file[256];
} GlobalSymbol;

typedef struct {
    GlobalSymbol syms[MAX_GLOBAL_SYMS];
    int          count;
} SymbolTable;

/* ============================================================
   RELOCATION (linker icin)
   ============================================================ */
typedef struct {
    uint32_t offset;        /* text/data section icindeki byte ofseti */
    char     section[8];
    int      type;          /* RelocType */
    char     symbol[MAX_NAME_LEN];
    int32_t  addend;
    int      resolved;
    /* linklemede hesaplanan */
    uint32_t sym_addr;
    uint32_t instr_addr;
} LReloc;

/* ============================================================
   YÜKLÜ OBJECT
   ============================================================ */
typedef struct {
    char     filename[256];
    uint32_t text_base;     /* bu objenin text section mutlak baslangic adresi */
    uint32_t data_base;     /* bu objenin data section mutlak baslangic adresi */
    int      text_count;    /* word */
    int      data_size;     /* byte */
    /* global semboller (isim + offset) */
    struct { char name[MAX_NAME_LEN]; char section[8]; uint32_t offset; } gsyms[256];
    int      gsym_count;
    /* local semboller */
    struct { char name[MAX_NAME_LEN]; char section[8]; uint32_t offset; } lsyms[512];
    int      lsym_count;
    /* relocs */
    LReloc   relocs[MAX_RELOCS];
    int      reloc_count;
} LoadedObject;

/* ============================================================
   LINKER DURUMU
   ============================================================ */
typedef struct {
    uint32_t text_base;
    uint32_t data_base;
    uint32_t stack_top;

    LoadedObject objects[MAX_OBJECTS];
    int          obj_count;

    uint32_t final_text[MAX_TOTAL_TEXT];
    int      final_text_count;

    uint8_t  final_data[MAX_TOTAL_DATA];
    int      final_data_size;

    SymbolTable sym_table;
} Linker;

static inline void linker_copy_cstr(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_size, "%s", src);
}

/* Fonksiyonlar */
void     linker_init(Linker* l, uint32_t text_base, uint32_t data_base, uint32_t stack_top);
int      linker_load_object(Linker* l, const char* path);
void     linker_pass1(Linker* l);
int      linker_pass2(Linker* l);
int      linker_write_hex(Linker* l, const char* path);
int      linker_write_mem(Linker* l, const char* path);
int      linker_write_bin(Linker* l, const char* path);
int      linker_write_map(Linker* l, const char* path);

extern const char* reloc_type_str[];
void apply_reloc(uint32_t* text_words, int text_count, LReloc* r, uint32_t sym_addr, uint32_t instr_addr);
uint32_t sym_resolve(SymbolTable* t, const char* name);

#endif /* LINKER_H */
