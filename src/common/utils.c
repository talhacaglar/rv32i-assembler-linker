#include "utils.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

typedef struct { const char* name; int num; } RegEntry;

RegEntry reg_table[] = {
    {"x0",0},{"x1",1},{"x2",2},{"x3",3},{"x4",4},{"x5",5},{"x6",6},{"x7",7},
    {"x8",8},{"x9",9},{"x10",10},{"x11",11},{"x12",12},{"x13",13},{"x14",14},{"x15",15},
    {"x16",16},{"x17",17},{"x18",18},{"x19",19},{"x20",20},{"x21",21},{"x22",22},{"x23",23},
    {"x24",24},{"x25",25},{"x26",26},{"x27",27},{"x28",28},{"x29",29},{"x30",30},{"x31",31},
    /* ABI isimleri */
    {"zero",0},{"ra",1},{"sp",2},{"gp",3},{"tp",4},
    {"t0",5},{"t1",6},{"t2",7},{"t3",28},{"t4",29},{"t5",30},{"t6",31},
    {"s0",8},{"fp",8},{"s1",9},
    {"s2",18},{"s3",19},{"s4",20},{"s5",21},{"s6",22},{"s7",23},
    {"s8",24},{"s9",25},{"s10",26},{"s11",27},
    {"a0",10},{"a1",11},{"a2",12},{"a3",13},{"a4",14},{"a5",15},{"a6",16},{"a7",17},
    {NULL,-1}
};

void copy_cstr(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_size, "%s", src);
}

int parse_reg(const char* s) {
    char buf[32];
    int i = 0;
    while (*s == ' ' || *s == '\t') s++;
    while (*s && *s != ' ' && *s != '\t' && *s != ',' && i < 31)
        buf[i++] = tolower((unsigned char)*s++);
    buf[i] = '\0';

    for (int j = 0; reg_table[j].name; j++)
        if (strcmp(buf, reg_table[j].name) == 0)
            return reg_table[j].num;

    fprintf(stderr, "[ERR] Bilinmeyen register: '%s'\n", buf);
    return -1;
}

void trim(char* s) {
    for (int i = (int)strlen(s)-1; i >= 0; i--) {
        if (s[i] == '\r' || s[i] == '\n' || s[i] == ' ' || s[i] == '\t')
            s[i] = '\0';
        else break;
    }

    int in_string = 0;
    int escaped = 0;
    for (char* p = s; *p; p++) {
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (*p == '\\' && in_string) {
            escaped = 1;
            continue;
        }
        if (*p == '"') {
            in_string = !in_string;
            continue;
        }
        if (!in_string && (*p == '#' || *p == ';')) {
            *p = '\0';
            break;
        }
    }

    for (int i = (int)strlen(s)-1; i >= 0; i--) {
        if (s[i] == ' ' || s[i] == '\t')
            s[i] = '\0';
        else break;
    }
}

void trim_leading(char** s) {
    while (**s == ' ' || **s == '\t') (*s)++;
}

int32_t parse_imm_or_label(const char* s, char* label_out, int* is_label) {
    char buf[MAX_NAME_LEN];
    int i = 0;
    while (*s == ' ' || *s == '\t') s++;
    while (*s && *s != ',' && *s != ' ' && i < MAX_NAME_LEN-1)
        buf[i++] = *s++;
    buf[i] = '\0';

    char* end;
    long v = strtol(buf, &end, 0);
    if (end != buf && *end == '\0') {
        *is_label = 0;
        return (int32_t)v;
    }
    if (buf[0]=='0' && (buf[1]=='x'||buf[1]=='X')) {
        *is_label = 0;
        return (int32_t)strtol(buf, NULL, 16);
    }
    *is_label = 1;
    copy_cstr(label_out, MAX_NAME_LEN, buf);
    return 0;
}

int parse_mem_operand(const char* s, int32_t* imm_out, int* rs1_out) {
    char tmp[MAX_NAME_LEN];
    copy_cstr(tmp, sizeof(tmp), s);
    char* p = strchr(tmp, '(');
    if (!p) goto invalid;
    *p = '\0';
    char* q = strchr(p+1, ')');
    if (!q) goto invalid;
    *q = '\0';
    for (char* tail = q + 1; *tail; tail++)
        if (*tail != ' ' && *tail != '\t') goto invalid;

    char* imm = tmp;
    while (*imm == ' ' || *imm == '\t') imm++;
    char* imm_end = imm + strlen(imm);
    while (imm_end > imm && (imm_end[-1] == ' ' || imm_end[-1] == '\t')) *--imm_end = '\0';
    long value = 0;
    if (*imm) {
        errno = 0;
        char* end = NULL;
        value = strtol(imm, &end, 0);
        if (errno == ERANGE || end == imm || *end != '\0' || value < INT32_MIN || value > INT32_MAX)
            goto invalid;
    }
    *rs1_out = parse_reg(p+1);
    if (*rs1_out < 0) goto invalid;
    *imm_out = (int32_t)value;
    return 1;

invalid:
    fprintf(stderr, "[ERR] Hatali bellek operandı: '%s'\n", s);
    *rs1_out = -1;
    return -1;
}
