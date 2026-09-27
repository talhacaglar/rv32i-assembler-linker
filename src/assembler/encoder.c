#include "assembler.h"
#include "encoder.h"
#include "parser.h"
#include "../common/utils.h"

uint32_t enc_R(int f7, int rs2, int rs1, int f3, int rd, int op) {
    return (((uint32_t)f7&0x7F)<<25)|(((uint32_t)rs2&0x1F)<<20)|(((uint32_t)rs1&0x1F)<<15)|
           (((uint32_t)f3&0x7)<<12)|(((uint32_t)rd&0x1F)<<7)|(op&0x7F);
}
uint32_t enc_I(int imm, int rs1, int f3, int rd, int op) {
    return (((uint32_t)imm&0xFFF)<<20)|(((uint32_t)rs1&0x1F)<<15)|(((uint32_t)f3&0x7)<<12)|
           (((uint32_t)rd&0x1F)<<7)|(op&0x7F);
}
uint32_t enc_S(int imm, int rs2, int rs1, int f3, int op) {
    int i11_5 = (imm>>5)&0x7F, i4_0 = imm&0x1F;
    return (((uint32_t)i11_5)<<25)|(((uint32_t)rs2&0x1F)<<20)|(((uint32_t)rs1&0x1F)<<15)|
           (((uint32_t)f3&0x7)<<12)|(((uint32_t)i4_0)<<7)|(op&0x7F);
}
uint32_t enc_B(int imm, int rs2, int rs1, int f3, int op) {
    imm &= 0x1FFF;
    int b12=(imm>>12)&1, b11=(imm>>11)&1, b10_5=(imm>>5)&0x3F, b4_1=(imm>>1)&0xF;
    return (((uint32_t)b12)<<31)|(((uint32_t)b10_5)<<25)|(((uint32_t)rs2&0x1F)<<20)|(((uint32_t)rs1&0x1F)<<15)|
           (((uint32_t)f3&0x7)<<12)|(((uint32_t)b4_1)<<8)|(((uint32_t)b11)<<7)|(op&0x7F);
}
uint32_t enc_U(int imm, int rd, int op) {
    return ((uint32_t)(imm&0xFFFFF000))|(((uint32_t)rd&0x1F)<<7)|(op&0x7F);
}
uint32_t enc_J(int imm, int rd, int op) {
    imm &= 0x1FFFFF;
    int b20=(imm>>20)&1, b19_12=(imm>>12)&0xFF, b11=(imm>>11)&1, b10_1=(imm>>1)&0x3FF;
    return (((uint32_t)b20)<<31)|(((uint32_t)b10_1)<<21)|(((uint32_t)b11)<<20)|(((uint32_t)b19_12)<<12)|(((uint32_t)rd&0x1F)<<7)|(op&0x7F);
}


void assemble_instr(Assembler* a, const char* mn, char* args_str, int line_num) {
    /* args dizisini ayır */
    char args[6][MAX_NAME_LEN] = {{0}};
    int argc = 0;
    char tmp[MAX_LINE_LEN];
    copy_cstr(tmp, sizeof(tmp), args_str);

    /* özel: imm(rs1) formatını bozmadan ayır */
    /* Önce virgülle böl ama parantez içine girme */
    {
        int depth = 0; int j = 0;
        char* p = tmp;
        while (*p && argc < 6) {
            if (*p == '(') depth++;
            else if (*p == ')') depth--;
            else if (*p == ',' && depth == 0) {
                args[argc][j] = '\0';
                /* trim */
                int k = 0; while (args[argc][k]==' '||args[argc][k]=='\t') k++;
                memmove(args[argc], args[argc]+k, strlen(args[argc]+k)+1);
                k = strlen(args[argc])-1;
                while (k>=0 && (args[argc][k]==' '||args[argc][k]=='\t')) args[argc][k--]='\0';
                argc++; j = 0; p++;
                continue;
            }
            if (j < MAX_NAME_LEN-1) args[argc][j++] = *p;
            p++;
        }
        if (argc >= 6) {
            fprintf(stderr, "  [ERR] Satir %d: Cok fazla operand\n", line_num);
            a->error_count++;
            return;
        }
        args[argc][j] = '\0';
        /* trim son arg */
        {
            int k = 0; while (args[argc][k]==' '||args[argc][k]=='\t') k++;
            memmove(args[argc], args[argc]+k, strlen(args[argc]+k)+1);
            k = strlen(args[argc])-1;
            while (k>=0 && (args[argc][k]==' '||args[argc][k]=='\t')) args[argc][k--]='\0';
        }
        if (args[argc][0]) argc++;
    }

    /* Validate arity before reading operand slots or generating output. */
    static const struct { const char* name; int min_args; int max_args; } arities[] = {
        {"add", 3, 3},
        {"sub", 3, 3},
        {"sll", 3, 3},
        {"slt", 3, 3},
        {"sltu", 3, 3},
        {"xor", 3, 3},
        {"srl", 3, 3},
        {"sra", 3, 3},
        {"or", 3, 3},
        {"and", 3, 3},
        {"addi", 3, 3},
        {"slti", 3, 3},
        {"sltiu", 3, 3},
        {"xori", 3, 3},
        {"ori", 3, 3},
        {"andi", 3, 3},
        {"slli", 3, 3},
        {"srli", 3, 3},
        {"srai", 3, 3},
        {"beq", 3, 3},
        {"bne", 3, 3},
        {"blt", 3, 3},
        {"bge", 3, 3},
        {"bltu", 3, 3},
        {"bgeu", 3, 3},
        {"lw", 2, 2},
        {"lh", 2, 2},
        {"lb", 2, 2},
        {"lhu", 2, 2},
        {"lbu", 2, 2},
        {"sw", 2, 2},
        {"sh", 2, 2},
        {"sb", 2, 2},
        {"lui", 2, 2},
        {"auipc", 2, 2},
        {"jal", 2, 2},
        {"mv", 2, 2},
        {"not", 2, 2},
        {"neg", 2, 2},
        {"seqz", 2, 2},
        {"snez", 2, 2},
        {"li", 2, 2},
        {"la", 2, 2},
        {"beqz", 2, 2},
        {"bnez", 2, 2},
        {"blez", 2, 2},
        {"bgez", 2, 2},
        {"bltz", 2, 2},
        {"bgtz", 2, 2},
        {"j", 1, 1},
        {"call", 1, 1},
        {"ecall", 0, 0},
        {"ebreak", 0, 0},
        {"nop", 0, 0},
        {"ret", 0, 0},
        {"jalr", 2, 3}, {"fence", 0, 2}
    };
    int known = 0;
    for (size_t i = 0; i < sizeof(arities) / sizeof(arities[0]); i++) {
        if (strcmp(mn, arities[i].name) != 0) continue;
        known = 1;
        if (argc < arities[i].min_args || argc > arities[i].max_args) {
            fprintf(stderr, "  [ERR] Satir %d: Hatali operand sayisi: %s\n", line_num, mn);
            a->error_count++;
            return;
        }
        break;
    }
    if (!known) {
        fprintf(stderr, "  [ERR] Satir %d: Bilinmeyen komut '%s'\n", line_num, mn);
        a->error_count++;
        return;
    }

    uint32_t off = (uint32_t)a->text_offset;

#define RD  (argc>0 ? parse_reg(args[0]) : 0)
#define RS1 (argc>1 ? parse_reg(args[1]) : 0)
#define RS2 (argc>2 ? parse_reg(args[2]) : 0)

    /* ---------- R-TYPE ---------- */
    if      (strcmp(mn,"add")==0)  emit_word(a, enc_R(0,RS2,RS1,0,RD,0x33));
    else if (strcmp(mn,"sub")==0)  emit_word(a, enc_R(0x20,RS2,RS1,0,RD,0x33));
    else if (strcmp(mn,"sll")==0)  emit_word(a, enc_R(0,RS2,RS1,1,RD,0x33));
    else if (strcmp(mn,"slt")==0)  emit_word(a, enc_R(0,RS2,RS1,2,RD,0x33));
    else if (strcmp(mn,"sltu")==0) emit_word(a, enc_R(0,RS2,RS1,3,RD,0x33));
    else if (strcmp(mn,"xor")==0)  emit_word(a, enc_R(0,RS2,RS1,4,RD,0x33));
    else if (strcmp(mn,"srl")==0)  emit_word(a, enc_R(0,RS2,RS1,5,RD,0x33));
    else if (strcmp(mn,"sra")==0)  emit_word(a, enc_R(0x20,RS2,RS1,5,RD,0x33));
    else if (strcmp(mn,"or")==0)   emit_word(a, enc_R(0,RS2,RS1,6,RD,0x33));
    else if (strcmp(mn,"and")==0)  emit_word(a, enc_R(0,RS2,RS1,7,RD,0x33));

    /* ---------- I-TYPE (arith) ---------- */
    else if (strcmp(mn,"addi")==0) {
        int rd=RD, rs1=RS1; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[2], lbl, &is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_LO12,lbl,0); imm=0; }
        emit_word(a, enc_I(imm,rs1,0,rd,0x13));
    }
    else if (strcmp(mn,"slti")==0)  { int rd=RD,rs1=RS1; emit_word(a,enc_I((int)strtol(args[2],NULL,0),rs1,2,rd,0x13)); }
    else if (strcmp(mn,"sltiu")==0) { int rd=RD,rs1=RS1; emit_word(a,enc_I((int)strtol(args[2],NULL,0),rs1,3,rd,0x13)); }
    else if (strcmp(mn,"xori")==0)  { int rd=RD,rs1=RS1; emit_word(a,enc_I((int)strtol(args[2],NULL,0),rs1,4,rd,0x13)); }
    else if (strcmp(mn,"ori")==0)   { int rd=RD,rs1=RS1; emit_word(a,enc_I((int)strtol(args[2],NULL,0),rs1,6,rd,0x13)); }
    else if (strcmp(mn,"andi")==0)  { int rd=RD,rs1=RS1; emit_word(a,enc_I((int)strtol(args[2],NULL,0),rs1,7,rd,0x13)); }
    else if (strcmp(mn,"slli")==0)  { int rd=RD,rs1=RS1,sh=(int)strtol(args[2],NULL,0)&0x1F; emit_word(a,enc_R(0,sh,rs1,1,rd,0x13)); }
    else if (strcmp(mn,"srli")==0)  { int rd=RD,rs1=RS1,sh=(int)strtol(args[2],NULL,0)&0x1F; emit_word(a,enc_R(0,sh,rs1,5,rd,0x13)); }
    else if (strcmp(mn,"srai")==0)  { int rd=RD,rs1=RS1,sh=(int)strtol(args[2],NULL,0)&0x1F; emit_word(a,enc_R(0x20,sh,rs1,5,rd,0x13)); }

    /* ---------- LOAD ---------- */
    else if (strcmp(mn,"lw")==0||strcmp(mn,"lh")==0||strcmp(mn,"lb")==0||
             strcmp(mn,"lhu")==0||strcmp(mn,"lbu")==0) {
        int rd=RD; int32_t imm; int rs1;
        parse_mem_operand(args[1], &imm, &rs1);
        int f3 = strcmp(mn,"lw")==0?2:strcmp(mn,"lh")==0?1:strcmp(mn,"lb")==0?0:
                 strcmp(mn,"lhu")==0?5:4;
        emit_word(a, enc_I(imm,rs1,f3,rd,0x03));
    }
    /* ---------- STORE ---------- */
    else if (strcmp(mn,"sw")==0||strcmp(mn,"sh")==0||strcmp(mn,"sb")==0) {
        int rs2=RD; /* args[0] kaynak reg */ int32_t imm; int rs1;
        parse_mem_operand(args[1], &imm, &rs1);
        int f3 = strcmp(mn,"sw")==0?2:strcmp(mn,"sh")==0?1:0;
        emit_word(a, enc_S(imm,rs2,rs1,f3,0x23));
    }
    /* ---------- BRANCH ---------- */
    else if (strcmp(mn,"beq")==0||strcmp(mn,"bne")==0||strcmp(mn,"blt")==0||
             strcmp(mn,"bge")==0||strcmp(mn,"bltu")==0||strcmp(mn,"bgeu")==0) {
        int rs1=RD, rs2=RS1;
        int f3 = strcmp(mn,"beq")==0?0:strcmp(mn,"bne")==0?1:strcmp(mn,"blt")==0?4:
                 strcmp(mn,"bge")==0?5:strcmp(mn,"bltu")==0?6:7;
        int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[2], lbl, &is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_BRANCH,lbl,0); imm=0; }
        emit_word(a, enc_B(imm,rs2,rs1,f3,0x63));
    }
    /* ---------- LUI / AUIPC ---------- */
    else if (strcmp(mn,"lui")==0) {
        int rd=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1], lbl, &is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_HI20,lbl,0); imm=0; }
        else imm = imm << 12;
        emit_word(a, enc_U(imm,rd,0x37));
    }
    else if (strcmp(mn,"auipc")==0) {
        int rd=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1], lbl, &is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_PCREL_HI20,lbl,0); imm=0; }
        else imm = imm << 12;
        emit_word(a, enc_U(imm,rd,0x17));
    }
    /* ---------- JAL / JALR ---------- */
    else if (strcmp(mn,"jal")==0) {
        int rd=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1], lbl, &is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_JAL,lbl,0); imm=0; }
        emit_word(a, enc_J(imm,rd,0x6F));
    }
    else if (strcmp(mn,"jalr")==0) {
        int rd=RD, rs1=RS1;
        int32_t imm = argc>2 ? (int32_t)strtol(args[2],NULL,0) : 0;
        emit_word(a, enc_I(imm,rs1,0,rd,0x67));
    }
    /* ---------- SYSTEM ---------- */
    else if (strcmp(mn,"ecall")==0)  emit_word(a, 0x00000073);
    else if (strcmp(mn,"ebreak")==0) emit_word(a, 0x00100073);
    else if (strcmp(mn,"fence")==0)  emit_word(a, 0x0000000F);

    /* ========== PSEUDO KOMUTLAR ========== */
    else if (strcmp(mn,"nop")==0) emit_word(a, enc_I(0,0,0,0,0x13));
    else if (strcmp(mn,"ret")==0) emit_word(a, enc_I(0,1,0,0,0x67));
    else if (strcmp(mn,"mv")==0)  { int rd=RD,rs1=RS1; emit_word(a,enc_I(0,rs1,0,rd,0x13)); }
    else if (strcmp(mn,"not")==0) { int rd=RD,rs1=RS1; emit_word(a,enc_I(-1,rs1,4,rd,0x13)); }
    else if (strcmp(mn,"neg")==0) { int rd=RD,rs1=RS1; emit_word(a,enc_R(0x20,rs1,0,0,rd,0x33)); }
    else if (strcmp(mn,"seqz")==0){ int rd=RD,rs1=RS1; emit_word(a,enc_I(1,rs1,3,rd,0x13)); }
    else if (strcmp(mn,"snez")==0){ int rd=RD,rs1=RS1; emit_word(a,enc_R(0,rs1,0,3,rd,0x33)); }

    else if (strcmp(mn,"j")==0) {
        int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[0], lbl, &is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_JAL,lbl,0); imm=0; }
        emit_word(a, enc_J(imm,0,0x6F));
    }
    else if (strcmp(mn,"li")==0) {
        int rd=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1], lbl, &is_l);
        if (is_l) {
            add_reloc(a,off,"text",RELOC_LI,lbl,0);
            emit_word(a, enc_U(0,rd,0x37));
            emit_word(a, enc_I(0,rd,0,rd,0x13));
        } else {
            if (imm >= -2048 && imm <= 2047) {
                emit_word(a, enc_I(imm,0,0,rd,0x13));
            } else {
                int32_t upper = (imm + 0x800) >> 12;
                int32_t lower = imm - (upper << 12);
                emit_word(a, enc_U(upper<<12,rd,0x37));
                emit_word(a, enc_I(lower,rd,0,rd,0x13));
            }
        }
    }
    else if (strcmp(mn,"la")==0) {
        int rd=RD; char* sym = args[1];
        while (*sym==' '||*sym=='\t') sym++;
        add_reloc(a,off,"text",RELOC_LA,sym,0);
        emit_word(a, enc_U(0,rd,0x17));  /* auipc placeholder */
        emit_word(a, enc_I(0,rd,0,rd,0x13)); /* addi placeholder */
    }
    else if (strcmp(mn,"call")==0) {
        char* sym = args[0];
        while (*sym==' '||*sym=='\t') sym++;
        add_reloc(a,off,"text",RELOC_CALL,sym,0);
        emit_word(a, enc_U(0,1,0x17));       /* auipc ra placeholder */
        emit_word(a, enc_I(0,1,0,1,0x67));   /* jalr ra,ra,0 placeholder */
    }
    else if (strcmp(mn,"beqz")==0) {
        int rs1=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1],lbl,&is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_BRANCH,lbl,0); imm=0; }
        emit_word(a,enc_B(imm,0,rs1,0,0x63));
    }
    else if (strcmp(mn,"bnez")==0) {
        int rs1=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1],lbl,&is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_BRANCH,lbl,0); imm=0; }
        emit_word(a,enc_B(imm,0,rs1,1,0x63));
    }
    else if (strcmp(mn,"blez")==0) {
        int rs1=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1],lbl,&is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_BRANCH,lbl,0); imm=0; }
        emit_word(a,enc_B(imm,rs1,0,5,0x63));
    }
    else if (strcmp(mn,"bgez")==0) {
        int rs1=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1],lbl,&is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_BRANCH,lbl,0); imm=0; }
        emit_word(a,enc_B(imm,0,rs1,5,0x63));
    }
    else if (strcmp(mn,"bltz")==0) {
        int rs1=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1],lbl,&is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_BRANCH,lbl,0); imm=0; }
        emit_word(a,enc_B(imm,0,rs1,4,0x63));
    }
    else if (strcmp(mn,"bgtz")==0) {
        int rs1=RD; int is_l; char lbl[MAX_NAME_LEN]={0};
        int32_t imm = parse_imm_or_label(args[1],lbl,&is_l);
        if (is_l) { add_reloc(a,off,"text",RELOC_BRANCH,lbl,0); imm=0; }
        emit_word(a,enc_B(imm,rs1,0,4,0x63));
    }
    else {
        fprintf(stderr, "  [WARN] Satir %d: Bilinmeyen komut '%s'\n", line_num, mn);
    }
}
