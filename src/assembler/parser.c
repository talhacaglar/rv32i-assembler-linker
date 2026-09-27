#include "assembler.h"
#include "parser.h"
#include "encoder.h"
#include "../common/utils.h"

const char* reloc_type_str[] = {
    "BRANCH","JAL","CALL","LA","LI","HI20","LO12","PCREL_HI20","ABS32"
};

static void parse_directive(char** p, char* dir, size_t dir_size, char** args) {
    int j = 0;
    while (**p && **p != ' ' && **p != '\t' && j < (int)dir_size - 1)
        dir[j++] = *(*p)++;
    dir[j] = '\0';
    *args = *p;
    trim_leading(args);
}

static void trim_token(char* s) {
    char* p = s;
    trim_leading(&p);
    if (p != s) memmove(s, p, strlen(p) + 1);
    for (int i = (int)strlen(s) - 1; i >= 0; i--) {
        if (s[i] == ' ' || s[i] == '\t')
            s[i] = '\0';
        else
            break;
    }
}

static void add_global(Assembler* a, const char* name) {
    char tmp[MAX_NAME_LEN];
    copy_cstr(tmp, sizeof(tmp), name);
    trim_token(tmp);
    if (!tmp[0]) return;

    for (int i = 0; i < a->obj.global_count; i++)
        if (strcmp(a->obj.globals[i], tmp) == 0) return;

    if (a->obj.global_count < MAX_GLOBALS)
        copy_cstr(a->obj.globals[a->obj.global_count++], MAX_NAME_LEN, tmp);
}

static void parse_global_list(Assembler* a, char* args) {
    char* tok = strtok(args, ",");
    while (tok) {
        add_global(a, tok);
        tok = strtok(NULL, ",");
    }
}

static char decode_escape(char c) {
    switch (c) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '0': return '\0';
        case '\\': return '\\';
        case '"': return '"';
        default: return c;
    }
}

static int string_literal_len(const char* args, int nul_terminated) {
    const char* p = strchr(args, '"');
    if (!p) return 0;
    p++;

    int len = 0;
    int escaped = 0;
    while (*p) {
        if (!escaped && *p == '"') break;
        if (!escaped && *p == '\\') {
            escaped = 1;
            p++;
            continue;
        }
        escaped = 0;
        len++;
        p++;
    }
    return len + (nul_terminated ? 1 : 0);
}

static void emit_string_literal(Assembler* a, const char* args, int nul_terminated) {
    const char* p = strchr(args, '"');
    if (!p) return;
    p++;

    int escaped = 0;
    while (*p) {
        if (!escaped && *p == '"') break;
        if (!escaped && *p == '\\') {
            escaped = 1;
            p++;
            continue;
        }
        emit_data_byte(a, (uint8_t)(escaped ? decode_escape(*p) : *p));
        escaped = 0;
        p++;
    }

    if (nul_terminated)
        emit_data_byte(a, 0);
}

void emit_word(Assembler* a, uint32_t w) {
    if (a->obj.text_count >= MAX_TEXT_WORDS) {
        fprintf(stderr, "[ERR] Text section doldu!\n"); return;
    }
    a->obj.text[a->obj.text_count++] = w;
    a->text_offset += 4;
}
void emit_data_byte(Assembler* a, uint8_t b) {
    if (a->obj.data_size >= MAX_DATA_BYTES) {
        fprintf(stderr, "[ERR] Data section doldu!\n"); return;
    }
    a->obj.data[a->obj.data_size++] = b;
    a->data_offset++;
}
void emit_data_word(Assembler* a, uint32_t w) {
    emit_data_byte(a, w & 0xFF);
    emit_data_byte(a, (w>>8) & 0xFF);
    emit_data_byte(a, (w>>16) & 0xFF);
    emit_data_byte(a, (w>>24) & 0xFF);
}

void add_reloc(Assembler* a, uint32_t offset, const char* section,
                      RelocType type, const char* sym, int32_t addend) {
    if (a->obj.reloc_count >= MAX_RELOCS) {
        fprintf(stderr, "[ERR] Reloc tablosu doldu!\n"); return;
    }
    Relocation* r = &a->obj.relocs[a->obj.reloc_count++];
    r->offset = offset;
    copy_cstr(r->section, sizeof(r->section), section);
    r->type = type;
    copy_cstr(r->symbol, sizeof(r->symbol), sym);
    r->addend = addend;
    r->resolved = 0;
}

void add_symbol(Assembler* a, const char* name, const char* section,
                       uint32_t offset, int is_global) {
    /* duplicate check */
    for (int i = 0; i < a->obj.sym_count; i++)
        if (strcmp(a->obj.symbols[i].name, name) == 0) return;
    if (a->obj.sym_count >= MAX_SYMBOLS) return;
    Symbol* s = &a->obj.symbols[a->obj.sym_count++];
    copy_cstr(s->name, sizeof(s->name), name);
    copy_cstr(s->section, sizeof(s->section), section);
    s->offset = offset;
    s->is_global = is_global;
}

void add_label_fp(Assembler* a, const char* name, const char* section, uint32_t off) {
    for (int i = 0; i < a->label_count; i++)
        if (strcmp(a->labels[i].name, name) == 0) return;
    if (a->label_count >= MAX_LABELS) return;
    Symbol* s = &a->labels[a->label_count++];
    copy_cstr(s->name, sizeof(s->name), name);
    copy_cstr(s->section, sizeof(s->section), section);
    s->offset = off;
}

Symbol* find_label(Assembler* a, const char* name) {
    for (int i = 0; i < a->label_count; i++)
        if (strcmp(a->labels[i].name, name) == 0)
            return &a->labels[i];
    return NULL;
}

int is_global_sym(Assembler* a, const char* name) {
    for (int i = 0; i < a->obj.global_count; i++)
        if (strcmp(a->obj.globals[i], name) == 0) return 1;
    return 0;
}


void process_directive(Assembler* a, const char* dir, char* args) {
    trim_leading(&args);

    if (strcmp(dir, ".text") == 0) {
        copy_cstr(a->current_section, sizeof(a->current_section), "text");
    }
    else if (strcmp(dir, ".data") == 0) {
        copy_cstr(a->current_section, sizeof(a->current_section), "data");
    }
    else if (strcmp(dir, ".global") == 0 || strcmp(dir, ".globl") == 0) {
        parse_global_list(a, args);
    }
    else if (strcmp(dir, ".word") == 0) {
        char* tok = strtok(args, ",");
        while (tok) {
            while (*tok == ' ') tok++;
            char* end;
            long v = strtol(tok, &end, 0);
            if (end != tok)
                emit_data_word(a, (uint32_t)v);
            else {
                /* label referansı */
                char sym[MAX_NAME_LEN];
                copy_cstr(sym, sizeof(sym), tok);
                trim_token(sym);
                add_reloc(a, a->data_offset, "data", RELOC_ABS32, sym, 0);
                emit_data_word(a, 0);
            }
            tok = strtok(NULL, ",");
        }
    }
    else if (strcmp(dir, ".byte") == 0) {
        char* tok = strtok(args, ",");
        while (tok) {
            emit_data_byte(a, (uint8_t)strtol(tok, NULL, 0));
            tok = strtok(NULL, ",");
        }
    }
    else if (strcmp(dir, ".half") == 0) {
        char* tok = strtok(args, ",");
        while (tok) {
            uint16_t v = (uint16_t)strtol(tok, NULL, 0);
            emit_data_byte(a, v & 0xFF);
            emit_data_byte(a, (v>>8) & 0xFF);
            tok = strtok(NULL, ",");
        }
    }
    else if (strcmp(dir, ".space") == 0 || strcmp(dir, ".skip") == 0) {
        int n = (int)strtol(args, NULL, 0);
        for (int i = 0; i < n; i++) emit_data_byte(a, 0);
    }
    else if (strcmp(dir, ".align") == 0) {
        int n = (int)strtol(args, NULL, 0);
        int align = 1 << n;
        if (strcmp(a->current_section, "data") == 0) {
            int pad = (-a->data_offset) & (align-1);
            for (int i = 0; i < pad; i++) emit_data_byte(a, 0);
        }
    }
    else if (strcmp(dir, ".string") == 0 || strcmp(dir, ".asciz") == 0) {
        emit_string_literal(a, args, 1);
    }
    else if (strcmp(dir, ".ascii") == 0) {
        emit_string_literal(a, args, 0);
    }
    else if (strcmp(dir, ".equ") == 0 || strcmp(dir, ".set") == 0) {
        /* .equ SYM, VAL - basit absolut sembol */
        char* comma = strchr(args, ',');
        if (comma) {
            *comma = '\0';
            char sym[MAX_NAME_LEN];
            copy_cstr(sym, sizeof(sym), args);
            trim_token(sym);
            /* add_label_fp(a, sym, "abs", (uint32_t)strtol(comma+1,NULL,0)); */
        }
    }
    /* Diger direktifler gormezden gelindi */
}

static int count_csv_items(const char* args) {
    while (*args == ' ' || *args == '\t') args++;
    if (!*args) return 0;

    int cnt = 1;
    for (const char* q = args; *q; q++)
        if (*q == ',') cnt++;
    return cnt;
}

static void first_pass_directive(Assembler* a, const char* dir, char* args,
                                 char* cur_sec, int* data_off) {
    if (strcmp(dir, ".text") == 0) {
        copy_cstr(cur_sec, 8, "text");
    }
    else if (strcmp(dir, ".data") == 0) {
        copy_cstr(cur_sec, 8, "data");
    }
    else if (strcmp(dir, ".global") == 0 || strcmp(dir, ".globl") == 0) {
        parse_global_list(a, args);
    }
    else if (strcmp(dir, ".word") == 0) {
        *data_off += 4 * count_csv_items(args);
    }
    else if (strcmp(dir, ".byte") == 0) {
        *data_off += count_csv_items(args);
    }
    else if (strcmp(dir, ".half") == 0) {
        *data_off += 2 * count_csv_items(args);
    }
    else if (strcmp(dir, ".space") == 0 || strcmp(dir, ".skip") == 0) {
        *data_off += (int)strtol(args, NULL, 0);
    }
    else if (strcmp(dir, ".string") == 0 || strcmp(dir, ".asciz") == 0) {
        *data_off += string_literal_len(args, 1);
    }
    else if (strcmp(dir, ".ascii") == 0) {
        *data_off += string_literal_len(args, 0);
    }
    else if (strcmp(dir, ".align") == 0) {
        int n = (int)strtol(args, NULL, 0);
        if (strcmp(cur_sec, "data") == 0 && n >= 0 && n < 31) {
            int align = 1 << n;
            int pad = (-*data_off) & (align - 1);
            *data_off += pad;
        }
    }
}


void first_pass(Assembler* a, char lines[][MAX_LINE_LEN], int line_count) {
    int text_off = 0, data_off = 0;
    char cur_sec[8] = "text";

    for (int i = 0; i < line_count; i++) {
        char buf[MAX_LINE_LEN];
        copy_cstr(buf, sizeof(buf), lines[i]);
        trim(buf);
        char* p = buf;
        trim_leading(&p);
        if (!*p) continue;

        /* directive? */
        if (*p == '.') {
            char dir[64]; char* args = NULL;
            parse_directive(&p, dir, sizeof(dir), &args);
            first_pass_directive(a, dir, args, cur_sec, &data_off);
            continue;
        }

        /* label var mi? */
        char* colon = strchr(p, ':');
        if (colon) {
            *colon = '\0';
            char lbl[MAX_NAME_LEN]; int j = 0;
            char* q = p;
            while (*q && *q != ' ' && j < MAX_NAME_LEN-1) lbl[j++] = *q++;
            lbl[j] = '\0';
            if (strcmp(cur_sec, "text") == 0)
                add_label_fp(a, lbl, "text", text_off);
            else
                add_label_fp(a, lbl, "data", data_off);
            p = colon + 1;
            trim_leading(&p);
            if (!*p) continue;
            if (*p == '.') {
                char dir[64]; char* args = NULL;
                parse_directive(&p, dir, sizeof(dir), &args);
                first_pass_directive(a, dir, args, cur_sec, &data_off);
                continue;
            }
        }

        /* komut boyutu tahmini */
        if (strcmp(cur_sec, "text") == 0) {
            char mn[32]; int j = 0;
            char* q = p;
            while (*q && *q != ' ' && *q != '\t' && j < 31)
                mn[j++] = (char)tolower((unsigned char)*q++);
            mn[j] = '\0';
            trim_leading(&q);

            if (strcmp(mn,"li")==0) {
                char* imm_arg = strchr(q, ',');
                if (imm_arg) {
                    imm_arg++;
                    int is_l = 0;
                    char lbl[MAX_NAME_LEN] = {0};
                    int32_t imm = parse_imm_or_label(imm_arg, lbl, &is_l);
                    text_off += (!is_l && imm >= -2048 && imm <= 2047) ? 4 : 8;
                } else {
                    text_off += 8;
                }
            }
            else if (strcmp(mn,"la")==0 || strcmp(mn,"call")==0)
                text_off += 8;
            else if (*mn && mn[0] != '.')
                text_off += 4;
        }
    }
}


void second_pass(Assembler* a, char lines[][MAX_LINE_LEN], int line_count) {
    for (int i = 0; i < line_count; i++) {
        char buf[MAX_LINE_LEN];
        copy_cstr(buf, sizeof(buf), lines[i]);
        trim(buf);
        char* p = buf;
        trim_leading(&p);
        if (!*p) continue;

        /* directive */
        if (*p == '.') {
            char dir[64]; char* args = NULL;
            parse_directive(&p, dir, sizeof(dir), &args);
            process_directive(a, dir, args);
            continue;
        }

        /* label var mi? */
        char* colon = strchr(p, ':');
        if (colon) {
            *colon = '\0';
            char lbl[MAX_NAME_LEN]; int j = 0;
            char* q = p;
            while (*q && *q != ' ' && j < MAX_NAME_LEN-1) lbl[j++] = *q++;
            lbl[j] = '\0';
            int glb = is_global_sym(a, lbl);
            if (strcmp(a->current_section,"text")==0)
                add_symbol(a, lbl, "text", (uint32_t)a->text_offset, glb);
            else
                add_symbol(a, lbl, "data", (uint32_t)a->data_offset, glb);
            p = colon + 1;
            trim_leading(&p);
            if (!*p) continue;
            if (*p == '.') {
                char dir[64]; char* args = NULL;
                parse_directive(&p, dir, sizeof(dir), &args);
                process_directive(a, dir, args);
                continue;
            }
        }

        if (strcmp(a->current_section,"text") != 0) continue;

        /* mnemonic + args */
        char mn[32]; int j = 0;
        char* q = p;
        while (*q && *q != ' ' && *q != '\t' && j < 31)
            mn[j++] = (char)tolower((unsigned char)*q++);
        mn[j] = '\0';
        trim_leading(&q);

        assemble_instr(a, mn, q, i+1);
    }
}


void resolve_local_relocs(Assembler* a) {
    for (int i = 0; i < a->obj.reloc_count; i++) {
        Relocation* r = &a->obj.relocs[i];
        Symbol* s = find_label(a, r->symbol);
        if (s) {
            r->resolved = 1;
            copy_cstr(r->target_section, sizeof(r->target_section), s->section);
            r->target_offset = s->offset;
        }
    }
}


int obj_save_json(ObjectFile* obj, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "[ERR] Dosya acilamadi: %s\n", path); return -1; }

    fprintf(f, "{\n");
    fprintf(f, "  \"filename\": \"%s\",\n", obj->filename);

    /* text */
    fprintf(f, "  \"text\": [");
    for (int i = 0; i < obj->text_count; i++) {
        if (i) fprintf(f, ",");
        fprintf(f, "%u", obj->text[i]);
    }
    fprintf(f, "],\n");

    /* data */
    fprintf(f, "  \"data\": [");
    for (int i = 0; i < obj->data_size; i++) {
        if (i) fprintf(f, ",");
        fprintf(f, "%d", obj->data[i]);
    }
    fprintf(f, "],\n");

    /* symbols */
    fprintf(f, "  \"symbols\": {");
    int first = 1;
    for (int i = 0; i < obj->sym_count; i++) {
        Symbol* s = &obj->symbols[i];
        if (!first) fprintf(f, ",");
        fprintf(f, "\n    \"%s\": {\"section\":\"%s\",\"offset\":%u,\"global\":%s}",
                s->name, s->section, s->offset, s->is_global?"true":"false");
        first = 0;
    }
    fprintf(f, "\n  },\n");

    /* relocations */
    fprintf(f, "  \"relocations\": [");
    for (int i = 0; i < obj->reloc_count; i++) {
        Relocation* r = &obj->relocs[i];
        if (i) fprintf(f, ",");
        fprintf(f, "\n    {\"offset\":%u,\"section\":\"%s\",\"type\":\"%s\","
                "\"symbol\":\"%s\",\"addend\":%d,\"resolved\":%s}",
                r->offset, r->section, reloc_type_str[r->type],
                r->symbol, r->addend, r->resolved?"true":"false");
    }
    fprintf(f, "\n  ],\n");

    /* globals */
    fprintf(f, "  \"globals\": [");
    for (int i = 0; i < obj->global_count; i++) {
        if (i) fprintf(f, ",");
        fprintf(f, "\"%s\"", obj->globals[i]);
    }
    fprintf(f, "]\n}\n");

    int failed = ferror(f);
    if (fclose(f) != 0) failed = 1;
    if (failed) fprintf(stderr, "[ERR] Obje yazilamadi: %s\n", path);
    return failed ? -1 : 0;
}
