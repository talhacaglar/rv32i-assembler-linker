#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================
   SABITLER
   ============================================================ */
#define MAX_LABELS       1024
#define MAX_RELOCS       2048
#define MAX_SYMBOLS      1024
#define MAX_GLOBALS      256
#define MAX_TEXT_WORDS   65536
#define MAX_DATA_BYTES   65536
#define MAX_LINE_LEN     512
#define MAX_NAME_LEN     128

/* ============================================================
   RELOCATION TIPLERI
   ============================================================ */
typedef enum {
    RELOC_BRANCH    = 0,
    RELOC_JAL       = 1,
    RELOC_CALL      = 2,
    RELOC_LA        = 3,
    RELOC_LI        = 4,
    RELOC_HI20      = 5,
    RELOC_LO12      = 6,
    RELOC_PCREL_HI20= 7,
    RELOC_ABS32     = 8,
} RelocType;

extern const char* reloc_type_str[];

/* ============================================================
   SEMBOL (label)
   ============================================================ */
typedef struct {
    char     name[MAX_NAME_LEN];
    char     section[8];   /* "text" veya "data" */
    uint32_t offset;
    int      is_global;
} Symbol;

/* ============================================================
   RELOCATION KAYDI
   ============================================================ */
typedef struct {
    uint32_t  offset;         /* section icindeki byte ofseti */
    char      section[8];     /* "text" veya "data" */
    RelocType type;
    char      symbol[MAX_NAME_LEN];
    int32_t   addend;
    int       resolved;
    char      target_section[8];
    uint32_t  target_offset;
} Relocation;

/* ============================================================
   OBJECT DOSYA
   ============================================================ */
typedef struct {
    char       filename[256];

    uint32_t   text[MAX_TEXT_WORDS];
    int        text_count;          /* word sayisi */

    uint8_t    data[MAX_DATA_BYTES];
    int        data_size;           /* byte sayisi */

    Symbol     symbols[MAX_SYMBOLS];
    int        sym_count;

    Relocation relocs[MAX_RELOCS];
    int        reloc_count;

    char       globals[MAX_GLOBALS][MAX_NAME_LEN];
    int        global_count;
} ObjectFile;

/* ============================================================
   ASSEMBLER DURUMU
   ============================================================ */
typedef struct {
    ObjectFile obj;
    char       current_section[8];
    int        text_offset;   /* byte */
    int        data_offset;   /* byte */
    /* first-pass label tablosu */
    Symbol     labels[MAX_LABELS];
    int        label_count;
    int        error_count;
} Assembler;

/* Fonksiyon bildirimleri */
void     asm_init(Assembler* a, const char* filename);
int      asm_assemble_file(Assembler* a, const char* path);
int      obj_save_json(ObjectFile* obj, const char* path);

#endif /* ASSEMBLER_H */
