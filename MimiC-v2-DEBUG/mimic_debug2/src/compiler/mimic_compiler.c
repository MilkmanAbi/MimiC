/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  MimiC - Self-Hosted C Compiler for RP2040/RP2350                         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Single-pass recursive descent compiler with direct ARM Thumb codegen     ║
 * ║  Optimized for minimal RAM usage on microcontrollers                      ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * 
 * Memory budget: ~20KB working memory for compilation
 * - 4KB input buffer (streaming)
 * - 4KB output buffer (streaming) 
 * - 4KB symbol table (~128 symbols)
 * - 4KB string table (identifiers/strings)
 * - 4KB type table + scratch
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "mimic.h"
#include "mimic_fat32.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

#define MC_INPUT_BUF    4096
#define MC_OUTPUT_BUF   4096
#define MC_MAX_SYMBOLS  128
#define MC_MAX_STRINGS  4096
#define MC_MAX_TYPES    64
#define MC_MAX_LOCALS   32
#define MC_MAX_BREAKS   16
#define MC_MAX_CONTS    16
#define MC_STACK_SIZE   256

// ============================================================================
// TOKEN TYPES
// ============================================================================

enum {
    // Single char tokens use their ASCII value
    
    // Keywords (128+)
    TK_INT = 128, TK_CHAR, TK_VOID, TK_SHORT, TK_LONG, TK_FLOAT, TK_DOUBLE,
    TK_SIGNED, TK_UNSIGNED, TK_CONST, TK_VOLATILE, TK_STATIC, TK_EXTERN,
    TK_AUTO, TK_REGISTER, TK_STRUCT, TK_UNION, TK_ENUM, TK_TYPEDEF,
    TK_IF, TK_ELSE, TK_WHILE, TK_DO, TK_FOR, TK_SWITCH, TK_CASE, TK_DEFAULT,
    TK_BREAK, TK_CONTINUE, TK_RETURN, TK_GOTO, TK_SIZEOF,
    
    // Multi-char operators
    TK_INC, TK_DEC,             // ++ --
    TK_SHL, TK_SHR,             // << >>
    TK_LE, TK_GE, TK_EQ, TK_NE, // <= >= == !=
    TK_AND, TK_OR,              // && ||
    TK_ADD_EQ, TK_SUB_EQ, TK_MUL_EQ, TK_DIV_EQ, TK_MOD_EQ,  // += -= *= /= %=
    TK_AND_EQ, TK_OR_EQ, TK_XOR_EQ, TK_SHL_EQ, TK_SHR_EQ,   // &= |= ^= <<= >>=
    TK_ARROW,                   // ->
    TK_ELLIPSIS,                // ...
    
    // Literals
    TK_NUM, TK_STR, TK_CHAR_LIT,
    
    // Identifier
    TK_IDENT,
    
    // End
    TK_EOF
};

// ============================================================================
// TYPE SYSTEM
// ============================================================================

enum {
    TY_VOID = 0,
    TY_CHAR,
    TY_SHORT, 
    TY_INT,
    TY_LONG,
    TY_FLOAT,
    TY_PTR,
    TY_ARRAY,
    TY_FUNC,
    TY_STRUCT,
    TY_UNION,
    TY_ENUM
};

typedef struct Type Type;
struct Type {
    uint8_t     kind;
    uint8_t     is_unsigned;
    uint8_t     is_const;
    uint8_t     align;
    uint16_t    size;
    Type*       base;       // For ptr/array/func
    uint16_t    array_len;  // For arrays
    uint16_t    param_count;// For funcs
    uint16_t    struct_id;  // For struct/union
};

// ============================================================================
// SYMBOL TABLE
// ============================================================================

enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_PARAM,
    SYM_LOCAL,
    SYM_TYPE,
    SYM_CONST
};

typedef struct Symbol Symbol;
struct Symbol {
    char        name[32];
    uint8_t     kind;
    uint8_t     scope;
    int16_t     offset;     // Stack offset for locals, address for globals
    Type*       type;
    Symbol*     next;       // Hash chain
};

// ============================================================================
// COMPILER STATE
// ============================================================================

typedef struct {
    // Input
    int         in_fd;
    uint8_t*    in_buf;
    uint32_t    in_pos;
    uint32_t    in_len;
    
    // Output  
    int         out_fd;
    uint8_t*    out_buf;
    uint32_t    out_pos;
    
    // Lexer state
    int         ch;
    int         tok;
    int         tok_val;
    char        tok_str[256];
    uint32_t    line;
    uint32_t    col;
    
    // Symbol table
    Symbol      symbols[MC_MAX_SYMBOLS];
    uint32_t    sym_count;
    Symbol*     sym_hash[64];
    uint8_t     scope;
    
    // Type table
    Type        types[MC_MAX_TYPES];
    uint32_t    type_count;
    Type*       ty_void;
    Type*       ty_char;
    Type*       ty_int;
    Type*       ty_long;
    
    // Local variables
    int16_t     local_offset;
    int16_t     param_offset;
    int16_t     max_local;
    
    // Break/continue targets
    uint32_t    break_targets[MC_MAX_BREAKS];
    uint32_t    cont_targets[MC_MAX_CONTS];
    int         break_count;
    int         cont_count;
    
    // Code generation
    uint32_t    code_pos;       // Current position in output
    uint32_t    data_pos;       // Data section start
    uint32_t    bss_pos;        // BSS section start
    
    // Error handling
    char        error[128];
    uint32_t    error_line;
    int         had_error;
    
    // Statistics
    uint32_t    tokens;
    uint32_t    bytes_out;
} Compiler;

static Compiler* cc;

// ============================================================================
// ERROR HANDLING
// ============================================================================

static void mc_error(const char* fmt, ...) {
    if (cc->had_error) return;
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(cc->error, sizeof(cc->error), fmt, args);
    va_end(args);
    
    cc->error_line = cc->line;
    cc->had_error = 1;
    
    printf("[ERROR] Line %lu: %s\n", (unsigned long)cc->line, cc->error);
}

// ============================================================================
// INPUT BUFFERING
// ============================================================================

static int mc_getc(void) {
    if (cc->in_pos >= cc->in_len) {
        int n = mimic_fread(cc->in_fd, cc->in_buf, MC_INPUT_BUF);
        if (n <= 0) return -1;
        cc->in_len = n;
        cc->in_pos = 0;
    }
    int c = cc->in_buf[cc->in_pos++];
    if (c == '\n') { cc->line++; cc->col = 0; }
    else cc->col++;
    return c;
}

static void mc_ungetc(int c) {
    if (c < 0) return;
    if (cc->in_pos > 0) {
        cc->in_pos--;
        cc->in_buf[cc->in_pos] = c;
        if (c == '\n') cc->line--;
    }
}

// ============================================================================
// OUTPUT BUFFERING
// ============================================================================

static void mc_flush(void) {
    if (cc->out_pos > 0) {
        mimic_fwrite(cc->out_fd, cc->out_buf, cc->out_pos);
        cc->bytes_out += cc->out_pos;
        cc->out_pos = 0;
    }
}

static void mc_emit8(uint8_t b) {
    if (cc->out_pos >= MC_OUTPUT_BUF) mc_flush();
    cc->out_buf[cc->out_pos++] = b;
    cc->code_pos++;
}

static void mc_emit16(uint16_t w) {
    mc_emit8(w & 0xFF);
    mc_emit8((w >> 8) & 0xFF);
}

static void mc_emit32(uint32_t d) {
    mc_emit16(d & 0xFFFF);
    mc_emit16((d >> 16) & 0xFFFF);
}

// ============================================================================
// LEXER
// ============================================================================

static const struct { const char* name; int tok; } keywords[] = {
    {"int", TK_INT}, {"char", TK_CHAR}, {"void", TK_VOID},
    {"short", TK_SHORT}, {"long", TK_LONG}, {"float", TK_FLOAT},
    {"double", TK_DOUBLE}, {"signed", TK_SIGNED}, {"unsigned", TK_UNSIGNED},
    {"const", TK_CONST}, {"volatile", TK_VOLATILE}, {"static", TK_STATIC},
    {"extern", TK_EXTERN}, {"auto", TK_AUTO}, {"register", TK_REGISTER},
    {"struct", TK_STRUCT}, {"union", TK_UNION}, {"enum", TK_ENUM},
    {"typedef", TK_TYPEDEF}, {"if", TK_IF}, {"else", TK_ELSE},
    {"while", TK_WHILE}, {"do", TK_DO}, {"for", TK_FOR},
    {"switch", TK_SWITCH}, {"case", TK_CASE}, {"default", TK_DEFAULT},
    {"break", TK_BREAK}, {"continue", TK_CONTINUE}, {"return", TK_RETURN},
    {"goto", TK_GOTO}, {"sizeof", TK_SIZEOF}, {NULL, 0}
};

static void mc_next(void) {
    while (1) {
        // Skip whitespace
        while (cc->ch >= 0 && cc->ch <= ' ') cc->ch = mc_getc();
        
        // Skip comments
        if (cc->ch == '/') {
            int c2 = mc_getc();
            if (c2 == '/') {
                // Line comment
                while (cc->ch >= 0 && cc->ch != '\n') cc->ch = mc_getc();
                continue;
            } else if (c2 == '*') {
                // Block comment
                int prev = 0;
                cc->ch = mc_getc();
                while (cc->ch >= 0 && !(prev == '*' && cc->ch == '/')) {
                    prev = cc->ch;
                    cc->ch = mc_getc();
                }
                cc->ch = mc_getc();
                continue;
            } else {
                mc_ungetc(c2);
            }
        }
        break;
    }
    
    if (cc->ch < 0) { cc->tok = TK_EOF; return; }
    
    cc->tokens++;
    
    // Number
    if (isdigit(cc->ch)) {
        cc->tok_val = 0;
        if (cc->ch == '0') {
            cc->ch = mc_getc();
            if (cc->ch == 'x' || cc->ch == 'X') {
                // Hex
                cc->ch = mc_getc();
                while (isxdigit(cc->ch)) {
                    int d = isdigit(cc->ch) ? cc->ch - '0' : 
                            (cc->ch | 32) - 'a' + 10;
                    cc->tok_val = cc->tok_val * 16 + d;
                    cc->ch = mc_getc();
                }
            } else if (isdigit(cc->ch)) {
                // Octal
                while (cc->ch >= '0' && cc->ch <= '7') {
                    cc->tok_val = cc->tok_val * 8 + (cc->ch - '0');
                    cc->ch = mc_getc();
                }
            }
        } else {
            // Decimal
            while (isdigit(cc->ch)) {
                cc->tok_val = cc->tok_val * 10 + (cc->ch - '0');
                cc->ch = mc_getc();
            }
        }
        // Skip suffix
        while (cc->ch == 'u' || cc->ch == 'U' || cc->ch == 'l' || cc->ch == 'L')
            cc->ch = mc_getc();
        cc->tok = TK_NUM;
        return;
    }
    
    // Identifier or keyword
    if (isalpha(cc->ch) || cc->ch == '_') {
        int len = 0;
        while ((isalnum(cc->ch) || cc->ch == '_') && len < 255) {
            cc->tok_str[len++] = cc->ch;
            cc->ch = mc_getc();
        }
        cc->tok_str[len] = 0;
        
        // Check keywords
        for (int i = 0; keywords[i].name; i++) {
            if (strcmp(cc->tok_str, keywords[i].name) == 0) {
                cc->tok = keywords[i].tok;
                return;
            }
        }
        cc->tok = TK_IDENT;
        return;
    }
    
    // String literal
    if (cc->ch == '"') {
        int len = 0;
        cc->ch = mc_getc();
        while (cc->ch >= 0 && cc->ch != '"' && len < 255) {
            if (cc->ch == '\\') {
                cc->ch = mc_getc();
                switch (cc->ch) {
                    case 'n': cc->tok_str[len++] = '\n'; break;
                    case 'r': cc->tok_str[len++] = '\r'; break;
                    case 't': cc->tok_str[len++] = '\t'; break;
                    case '0': cc->tok_str[len++] = '\0'; break;
                    case '\\': cc->tok_str[len++] = '\\'; break;
                    case '"': cc->tok_str[len++] = '"'; break;
                    default: cc->tok_str[len++] = cc->ch; break;
                }
            } else {
                cc->tok_str[len++] = cc->ch;
            }
            cc->ch = mc_getc();
        }
        cc->tok_str[len] = 0;
        if (cc->ch == '"') cc->ch = mc_getc();
        cc->tok = TK_STR;
        return;
    }
    
    // Character literal
    if (cc->ch == '\'') {
        cc->ch = mc_getc();
        if (cc->ch == '\\') {
            cc->ch = mc_getc();
            switch (cc->ch) {
                case 'n': cc->tok_val = '\n'; break;
                case 'r': cc->tok_val = '\r'; break;
                case 't': cc->tok_val = '\t'; break;
                case '0': cc->tok_val = '\0'; break;
                default: cc->tok_val = cc->ch; break;
            }
        } else {
            cc->tok_val = cc->ch;
        }
        cc->ch = mc_getc();
        if (cc->ch == '\'') cc->ch = mc_getc();
        cc->tok = TK_CHAR_LIT;
        return;
    }
    
    // Operators
    int c = cc->ch;
    cc->ch = mc_getc();
    
    switch (c) {
        case '+':
            if (cc->ch == '+') { cc->ch = mc_getc(); cc->tok = TK_INC; }
            else if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_ADD_EQ; }
            else cc->tok = '+';
            break;
        case '-':
            if (cc->ch == '-') { cc->ch = mc_getc(); cc->tok = TK_DEC; }
            else if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_SUB_EQ; }
            else if (cc->ch == '>') { cc->ch = mc_getc(); cc->tok = TK_ARROW; }
            else cc->tok = '-';
            break;
        case '*':
            if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_MUL_EQ; }
            else cc->tok = '*';
            break;
        case '/':
            if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_DIV_EQ; }
            else cc->tok = '/';
            break;
        case '%':
            if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_MOD_EQ; }
            else cc->tok = '%';
            break;
        case '&':
            if (cc->ch == '&') { cc->ch = mc_getc(); cc->tok = TK_AND; }
            else if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_AND_EQ; }
            else cc->tok = '&';
            break;
        case '|':
            if (cc->ch == '|') { cc->ch = mc_getc(); cc->tok = TK_OR; }
            else if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_OR_EQ; }
            else cc->tok = '|';
            break;
        case '^':
            if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_XOR_EQ; }
            else cc->tok = '^';
            break;
        case '<':
            if (cc->ch == '<') {
                cc->ch = mc_getc();
                if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_SHL_EQ; }
                else cc->tok = TK_SHL;
            } else if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_LE; }
            else cc->tok = '<';
            break;
        case '>':
            if (cc->ch == '>') {
                cc->ch = mc_getc();
                if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_SHR_EQ; }
                else cc->tok = TK_SHR;
            } else if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_GE; }
            else cc->tok = '>';
            break;
        case '=':
            if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_EQ; }
            else cc->tok = '=';
            break;
        case '!':
            if (cc->ch == '=') { cc->ch = mc_getc(); cc->tok = TK_NE; }
            else cc->tok = '!';
            break;
        case '.':
            if (cc->ch == '.' && mc_getc() == '.') {
                cc->ch = mc_getc();
                cc->tok = TK_ELLIPSIS;
            } else {
                cc->tok = '.';
            }
            break;
        default:
            cc->tok = c;
            break;
    }
}

static void mc_expect(int tok) {
    if (cc->tok != tok) {
        mc_error("Expected '%c', got '%c'", tok, cc->tok);
    }
    mc_next();
}

// ============================================================================
// TYPE MANAGEMENT
// ============================================================================

static Type* mc_type_new(int kind, int size, int align) {
    if (cc->type_count >= MC_MAX_TYPES) {
        mc_error("Too many types");
        return cc->ty_int;
    }
    Type* t = &cc->types[cc->type_count++];
    memset(t, 0, sizeof(Type));
    t->kind = kind;
    t->size = size;
    t->align = align;
    return t;
}

static Type* mc_type_ptr(Type* base) {
    Type* t = mc_type_new(TY_PTR, 4, 4);
    t->base = base;
    return t;
}

static Type* mc_type_array(Type* base, int len) {
    Type* t = mc_type_new(TY_ARRAY, base->size * len, base->align);
    t->base = base;
    t->array_len = len;
    return t;
}

static int mc_type_size(Type* t) {
    if (!t) return 4;
    return t->size;
}

static int mc_type_is_ptr(Type* t) {
    return t && (t->kind == TY_PTR || t->kind == TY_ARRAY);
}

static int mc_type_is_int(Type* t) {
    return t && t->kind >= TY_CHAR && t->kind <= TY_LONG;
}

// ============================================================================
// SYMBOL TABLE
// ============================================================================

static uint32_t mc_hash(const char* s) {
    uint32_t h = 0;
    while (*s) h = h * 31 + *s++;
    return h & 63;
}

static Symbol* mc_sym_find(const char* name) {
    uint32_t h = mc_hash(name);
    for (Symbol* s = cc->sym_hash[h]; s; s = s->next) {
        if (strcmp(s->name, name) == 0 && s->scope <= cc->scope) {
            return s;
        }
    }
    return NULL;
}

static Symbol* mc_sym_add(const char* name, int kind, Type* type) {
    if (cc->sym_count >= MC_MAX_SYMBOLS) {
        mc_error("Too many symbols");
        return NULL;
    }
    
    Symbol* s = &cc->symbols[cc->sym_count++];
    strncpy(s->name, name, 31);
    s->name[31] = 0;
    s->kind = kind;
    s->scope = cc->scope;
    s->type = type;
    s->offset = 0;
    
    uint32_t h = mc_hash(name);
    s->next = cc->sym_hash[h];
    cc->sym_hash[h] = s;
    
    return s;
}

static void mc_scope_enter(void) {
    cc->scope++;
}

static void mc_scope_leave(void) {
    // Remove symbols from hash table
    for (int i = 0; i < 64; i++) {
        while (cc->sym_hash[i] && cc->sym_hash[i]->scope == cc->scope) {
            cc->sym_hash[i] = cc->sym_hash[i]->next;
        }
    }
    cc->scope--;
}

// ============================================================================
// ARM THUMB CODE GENERATION
// ============================================================================

// Register allocation: r0-r3 = args/scratch, r4-r7 = saved
// r7 = frame pointer, sp = stack pointer, lr = link register

// Thumb instruction encoders
static void mc_thumb_mov_imm8(int rd, int imm) {
    // MOV Rd, #imm8 (001 00 ddd iiiiiiii)
    mc_emit16(0x2000 | (rd << 8) | (imm & 0xFF));
}

static void mc_thumb_mov_reg(int rd, int rs) {
    // MOV Rd, Rs (high regs: 0100 0110 D ddd ssss)
    if (rd > 7 || rs > 7) {
        mc_emit16(0x4600 | ((rd & 8) << 4) | ((rd & 7) << 0) | ((rs & 7) << 3) | ((rs & 8) << 3));
    } else {
        // ADD Rd, Rs, #0
        mc_emit16(0x1C00 | rs | (rd << 3));
    }
}

static void mc_thumb_add_imm8(int rd, int imm) {
    // ADD Rd, #imm8 (001 10 ddd iiiiiiii)
    mc_emit16(0x3000 | (rd << 8) | (imm & 0xFF));
}

static void mc_thumb_sub_imm8(int rd, int imm) {
    // SUB Rd, #imm8 (001 11 ddd iiiiiiii)
    mc_emit16(0x3800 | (rd << 8) | (imm & 0xFF));
}

static void mc_thumb_add_reg(int rd, int rn, int rm) {
    // ADD Rd, Rn, Rm (0001 100 mmm nnn ddd)
    mc_emit16(0x1800 | rm | (rn << 3) | (rd << 6));
}

static void mc_thumb_sub_reg(int rd, int rn, int rm) {
    // SUB Rd, Rn, Rm (0001 101 mmm nnn ddd)
    mc_emit16(0x1A00 | rm | (rn << 3) | (rd << 6));
}

static void mc_thumb_mul(int rd, int rm) {
    // MUL Rd, Rm (0100 001 101 mmm ddd)
    mc_emit16(0x4340 | rm | (rd << 3));
}

static void mc_thumb_and_reg(int rd, int rm) {
    mc_emit16(0x4000 | rm | (rd << 3));
}

static void mc_thumb_orr_reg(int rd, int rm) {
    mc_emit16(0x4300 | rm | (rd << 3));
}

static void mc_thumb_eor_reg(int rd, int rm) {
    mc_emit16(0x4040 | rm | (rd << 3));
}

static void mc_thumb_mvn(int rd, int rm) {
    mc_emit16(0x43C0 | rm | (rd << 3));
}

static void mc_thumb_neg(int rd, int rm) {
    mc_emit16(0x4240 | rm | (rd << 3));
}

static void mc_thumb_lsl_imm(int rd, int rm, int imm) {
    mc_emit16(0x0000 | (imm << 6) | (rm << 3) | rd);
}

static void mc_thumb_lsr_imm(int rd, int rm, int imm) {
    mc_emit16(0x0800 | (imm << 6) | (rm << 3) | rd);
}

static void mc_thumb_asr_imm(int rd, int rm, int imm) {
    mc_emit16(0x1000 | (imm << 6) | (rm << 3) | rd);
}

static void mc_thumb_lsl_reg(int rd, int rs) {
    mc_emit16(0x4080 | rs | (rd << 3));
}

static void mc_thumb_lsr_reg(int rd, int rs) {
    mc_emit16(0x40C0 | rs | (rd << 3));
}

static void mc_thumb_cmp_imm8(int rn, int imm) {
    mc_emit16(0x2800 | (rn << 8) | (imm & 0xFF));
}

static void mc_thumb_cmp_reg(int rn, int rm) {
    mc_emit16(0x4280 | rm | (rn << 3));
}

static void mc_thumb_ldr_sp(int rt, int imm) {
    // LDR Rt, [SP, #imm] (1001 1 ttt iiiiiiii) imm is in words
    mc_emit16(0x9800 | (rt << 8) | ((imm >> 2) & 0xFF));
}

static void mc_thumb_str_sp(int rt, int imm) {
    // STR Rt, [SP, #imm] (1001 0 ttt iiiiiiii)
    mc_emit16(0x9000 | (rt << 8) | ((imm >> 2) & 0xFF));
}

static void mc_thumb_ldr_reg(int rt, int rn, int rm) {
    mc_emit16(0x5800 | rm | (rn << 3) | (rt << 6));
}

static void mc_thumb_str_reg(int rt, int rn, int rm) {
    mc_emit16(0x5000 | rm | (rn << 3) | (rt << 6));
}

static void mc_thumb_ldr_imm(int rt, int rn, int imm) {
    // LDR Rt, [Rn, #imm5*4]
    mc_emit16(0x6800 | ((imm >> 2) << 6) | (rn << 3) | rt);
}

static void mc_thumb_str_imm(int rt, int rn, int imm) {
    // STR Rt, [Rn, #imm5*4]
    mc_emit16(0x6000 | ((imm >> 2) << 6) | (rn << 3) | rt);
}

static void mc_thumb_ldrb_imm(int rt, int rn, int imm) {
    mc_emit16(0x7800 | (imm << 6) | (rn << 3) | rt);
}

static void mc_thumb_strb_imm(int rt, int rn, int imm) {
    mc_emit16(0x7000 | (imm << 6) | (rn << 3) | rt);
}

static void mc_thumb_ldrh_imm(int rt, int rn, int imm) {
    mc_emit16(0x8800 | ((imm >> 1) << 6) | (rn << 3) | rt);
}

static void mc_thumb_strh_imm(int rt, int rn, int imm) {
    mc_emit16(0x8000 | ((imm >> 1) << 6) | (rn << 3) | rt);
}

static void mc_thumb_push(uint8_t regs, int lr) {
    mc_emit16(0xB400 | (lr << 8) | regs);
}

static void mc_thumb_pop(uint8_t regs, int pc) {
    mc_emit16(0xBC00 | (pc << 8) | regs);
}

static void mc_thumb_add_sp_imm(int imm) {
    // ADD SP, #imm (imm in words, positive)
    mc_emit16(0xB000 | ((imm >> 2) & 0x7F));
}

static void mc_thumb_sub_sp_imm(int imm) {
    // SUB SP, #imm (imm in words)
    mc_emit16(0xB080 | ((imm >> 2) & 0x7F));
}

static void mc_thumb_b(int offset) {
    // B offset (11100 ooooooooooo) offset in halfwords
    mc_emit16(0xE000 | ((offset >> 1) & 0x7FF));
}

static uint32_t mc_thumb_b_placeholder(void) {
    uint32_t pos = cc->code_pos;
    mc_emit16(0xE000);  // Will be patched
    return pos;
}

static void mc_thumb_b_patch(uint32_t pos, uint32_t target) {
    (void)pos; (void)target;
    // TODO: implement proper patching
    // int32_t offset = (int32_t)(target - pos - 4);
}

static void mc_thumb_bcc(int cond, int offset) {
    // Bcc offset (1101 cccc oooooooo) offset in halfwords
    mc_emit16(0xD000 | (cond << 8) | ((offset >> 1) & 0xFF));
}

static uint32_t mc_thumb_bcc_placeholder(int cond) {
    uint32_t pos = cc->code_pos;
    mc_emit16(0xD000 | (cond << 8));
    return pos;
}

static void mc_thumb_bl(int offset) {
    // BL offset - 32-bit instruction
    int32_t off = offset >> 1;
    uint16_t hi = 0xF000 | ((off >> 11) & 0x7FF);
    uint16_t lo = 0xF800 | (off & 0x7FF);
    mc_emit16(hi);
    mc_emit16(lo);
}

static void mc_thumb_bx(int rm) {
    mc_emit16(0x4700 | (rm << 3));
}

static void mc_thumb_blx(int rm) {
    mc_emit16(0x4780 | (rm << 3));
}

static void mc_thumb_svc(int imm) {
    mc_emit16(0xDF00 | (imm & 0xFF));
}

// Condition codes
#define CC_EQ 0
#define CC_NE 1
#define CC_CS 2
#define CC_CC 3
#define CC_MI 4
#define CC_PL 5
#define CC_VS 6
#define CC_VC 7
#define CC_HI 8
#define CC_LS 9
#define CC_GE 10
#define CC_LT 11
#define CC_GT 12
#define CC_LE 13
#define CC_AL 14

// ============================================================================
// EXPRESSION CODEGEN
// ============================================================================

// Current register allocation (simple: always use r0-r3)
static int mc_reg = 0;

static void mc_load_imm(int val) {
    if (val >= 0 && val <= 255) {
        mc_thumb_mov_imm8(mc_reg, val);
    } else if (val >= -128 && val < 0) {
        mc_thumb_mov_imm8(mc_reg, -val);
        mc_thumb_neg(mc_reg, mc_reg);
    } else {
        // Load from literal pool or synthesize
        // For now, use multiple adds
        mc_thumb_mov_imm8(mc_reg, 0);
        int v = val;
        int shift = 0;
        while (v) {
            if (v & 0xFF) {
                if (shift) mc_thumb_lsl_imm(mc_reg, mc_reg, shift);
                mc_thumb_add_imm8(mc_reg, v & 0xFF);
                shift = 0;
            }
            v >>= 8;
            shift += 8;
        }
    }
}

static Type* mc_expr(void);
static Type* mc_expr_assign(void);
static Type* mc_expr_unary(void);

static Type* mc_expr_primary(void) {
    Type* ty = cc->ty_int;
    
    if (cc->tok == TK_NUM) {
        mc_load_imm(cc->tok_val);
        mc_next();
        return cc->ty_int;
    }
    
    if (cc->tok == TK_CHAR_LIT) {
        mc_load_imm(cc->tok_val);
        mc_next();
        return cc->ty_char;
    }
    
    if (cc->tok == TK_STR) {
        // String literal - store in data section
        // For now, just return pointer type
        mc_load_imm(0);  // TODO: actual string address
        mc_next();
        return mc_type_ptr(cc->ty_char);
    }
    
    if (cc->tok == TK_IDENT) {
        Symbol* sym = mc_sym_find(cc->tok_str);
        if (!sym) {
            mc_error("Undefined symbol: %s", cc->tok_str);
            mc_next();
            return cc->ty_int;
        }
        
        mc_next();
        
        if (cc->tok == '(') {
            // Function call
            mc_next();
            int nargs = 0;
            int saved_reg = mc_reg;
            
            while (cc->tok != ')' && cc->tok != TK_EOF) {
                mc_reg = nargs;
                mc_expr_assign();
                nargs++;
                if (cc->tok == ',') mc_next();
            }
            mc_expect(')');
            
            // Call function (BL or BLX)
            // For now, emit SVC for syscalls
            mc_thumb_blx(0);  // TODO: actual function address
            
            mc_reg = saved_reg;
            if (sym->type && sym->type->kind == TY_FUNC && sym->type->base) {
                return sym->type->base;  // Return type
            }
            return cc->ty_int;
        }
        
        // Variable access
        if (sym->kind == SYM_LOCAL || sym->kind == SYM_PARAM) {
            mc_thumb_ldr_sp(mc_reg, sym->offset);
        } else {
            // Global - load address then load value
            mc_load_imm(sym->offset);
            mc_thumb_ldr_imm(mc_reg, mc_reg, 0);
        }
        
        return sym->type ? sym->type : cc->ty_int;
    }
    
    if (cc->tok == '(') {
        mc_next();
        
        // Check for cast
        if (cc->tok == TK_INT || cc->tok == TK_CHAR || cc->tok == TK_VOID ||
            cc->tok == TK_SHORT || cc->tok == TK_LONG) {
            // Parse type and cast
            mc_next();
            while (cc->tok == '*') mc_next();
            mc_expect(')');
            return mc_expr_unary();
        }
        
        ty = mc_expr();
        mc_expect(')');
        return ty;
    }
    
    mc_error("Expected expression");
    mc_next();
    return cc->ty_int;
}

static Type* mc_expr_postfix(void) {
    Type* ty = mc_expr_primary();
    
    while (1) {
        if (cc->tok == '[') {
            // Array subscript
            mc_next();
            mc_thumb_push(1 << mc_reg, 0);
            mc_reg++;
            mc_expr();  // index expression
            mc_reg--;
            mc_thumb_pop(1 << (mc_reg + 1), 0);
            
            // Calculate offset
            int elem_size = ty->base ? mc_type_size(ty->base) : 4;
            if (elem_size > 1) {
                mc_load_imm(elem_size);
                mc_thumb_mul(mc_reg, mc_reg);
            }
            mc_thumb_add_reg(mc_reg, mc_reg, mc_reg + 1);
            mc_thumb_ldr_imm(mc_reg, mc_reg, 0);
            
            mc_expect(']');
            ty = ty->base ? ty->base : cc->ty_int;
        }
        else if (cc->tok == TK_INC || cc->tok == TK_DEC) {
            // Post increment/decrement
            int is_inc = cc->tok == TK_INC;
            mc_next();
            // TODO: proper implementation
            if (is_inc) mc_thumb_add_imm8(mc_reg, 1);
            else mc_thumb_sub_imm8(mc_reg, 1);
        }
        else if (cc->tok == '.') {
            mc_next();
            // Struct member access
            mc_next();  // Skip member name
            // TODO: proper implementation
        }
        else if (cc->tok == TK_ARROW) {
            mc_next();
            // Pointer struct member access
            mc_next();  // Skip member name
            // TODO: proper implementation
        }
        else {
            break;
        }
    }
    
    return ty;
}

static Type* mc_expr_unary(void) {
    if (cc->tok == '-') {
        mc_next();
        Type* ty = mc_expr_unary();
        mc_thumb_neg(mc_reg, mc_reg);
        return ty;
    }
    if (cc->tok == '!') {
        mc_next();
        mc_expr_unary();
        mc_thumb_cmp_imm8(mc_reg, 0);
        mc_thumb_mov_imm8(mc_reg, 0);
        // Set to 1 if was 0
        mc_thumb_bcc(CC_NE, 2);
        mc_thumb_mov_imm8(mc_reg, 1);
        return cc->ty_int;
    }
    if (cc->tok == '~') {
        mc_next();
        Type* ty = mc_expr_unary();
        mc_thumb_mvn(mc_reg, mc_reg);
        return ty;
    }
    if (cc->tok == '*') {
        mc_next();
        Type* ty = mc_expr_unary();
        mc_thumb_ldr_imm(mc_reg, mc_reg, 0);
        return ty->base ? ty->base : cc->ty_int;
    }
    if (cc->tok == '&') {
        mc_next();
        // Address-of
        if (cc->tok == TK_IDENT) {
            Symbol* sym = mc_sym_find(cc->tok_str);
            if (sym) {
                if (sym->kind == SYM_LOCAL || sym->kind == SYM_PARAM) {
                    // LEA: SP + offset
                    mc_thumb_mov_reg(mc_reg, 13);  // SP
                    mc_thumb_add_imm8(mc_reg, sym->offset);
                } else {
                    mc_load_imm(sym->offset);
                }
            }
            mc_next();
            return mc_type_ptr(sym ? sym->type : cc->ty_int);
        }
    }
    if (cc->tok == TK_INC || cc->tok == TK_DEC) {
        int is_inc = cc->tok == TK_INC;
        mc_next();
        Type* ty = mc_expr_unary();
        if (is_inc) mc_thumb_add_imm8(mc_reg, 1);
        else mc_thumb_sub_imm8(mc_reg, 1);
        return ty;
    }
    if (cc->tok == TK_SIZEOF) {
        mc_next();
        mc_expect('(');
        // Parse type or expression
        Type* ty = cc->ty_int;
        if (cc->tok == TK_INT) { ty = cc->ty_int; mc_next(); }
        else if (cc->tok == TK_CHAR) { ty = cc->ty_char; mc_next(); }
        else if (cc->tok == TK_VOID) { ty = cc->ty_void; mc_next(); }
        else if (cc->tok == TK_LONG) { ty = cc->ty_long; mc_next(); }
        while (cc->tok == '*') { ty = mc_type_ptr(ty); mc_next(); }
        mc_expect(')');
        mc_load_imm(mc_type_size(ty));
        return cc->ty_int;
    }
    
    return mc_expr_postfix();
}

static Type* mc_expr_mul(void) {
    Type* ty = mc_expr_unary();
    
    while (cc->tok == '*' || cc->tok == '/' || cc->tok == '%') {
        int op = cc->tok;
        mc_next();
        
        mc_thumb_push(1 << mc_reg, 0);
        mc_reg++;
        mc_expr_unary();
        mc_reg--;
        mc_thumb_pop(1 << (mc_reg + 1), 0);
        
        if (op == '*') {
            mc_thumb_mul(mc_reg, mc_reg + 1);
        } else {
            // Division needs library call
            mc_thumb_svc(op == '/' ? 1 : 2);  // div/mod syscall
        }
    }
    
    return ty;
}

static Type* mc_expr_add(void) {
    Type* ty = mc_expr_mul();
    
    while (cc->tok == '+' || cc->tok == '-') {
        int op = cc->tok;
        mc_next();
        
        mc_thumb_push(1 << mc_reg, 0);
        mc_reg++;
        mc_expr_mul();
        mc_reg--;
        mc_thumb_pop(1 << (mc_reg + 1), 0);
        
        if (op == '+') {
            mc_thumb_add_reg(mc_reg, mc_reg, mc_reg + 1);
        } else {
            mc_thumb_sub_reg(mc_reg, mc_reg + 1, mc_reg);
        }
    }
    
    return ty;
}

static Type* mc_expr_shift(void) {
    Type* ty = mc_expr_add();
    
    while (cc->tok == TK_SHL || cc->tok == TK_SHR) {
        int op = cc->tok;
        mc_next();
        
        mc_thumb_push(1 << mc_reg, 0);
        mc_reg++;
        mc_expr_add();
        mc_reg--;
        mc_thumb_pop(1 << (mc_reg + 1), 0);
        
        if (op == TK_SHL) mc_thumb_lsl_reg(mc_reg + 1, mc_reg);
        else mc_thumb_lsr_reg(mc_reg + 1, mc_reg);
        mc_thumb_mov_reg(mc_reg, mc_reg + 1);
    }
    
    return ty;
}

static Type* mc_expr_rel(void) {
    Type* ty = mc_expr_shift();
    
    while (cc->tok == '<' || cc->tok == '>' || cc->tok == TK_LE || cc->tok == TK_GE) {
        int op = cc->tok;
        mc_next();
        
        mc_thumb_push(1 << mc_reg, 0);
        mc_reg++;
        mc_expr_shift();
        mc_reg--;
        mc_thumb_pop(1 << (mc_reg + 1), 0);
        
        mc_thumb_cmp_reg(mc_reg + 1, mc_reg);
        mc_thumb_mov_imm8(mc_reg, 0);
        
        int cond;
        switch (op) {
            case '<':   cond = CC_LT; break;
            case '>':   cond = CC_GT; break;
            case TK_LE: cond = CC_LE; break;
            case TK_GE: cond = CC_GE; break;
            default:    cond = CC_AL; break;
        }
        mc_thumb_bcc(cond, 2);
        mc_thumb_mov_imm8(mc_reg, 1);
        ty = cc->ty_int;
    }
    
    return ty;
}

static Type* mc_expr_eq(void) {
    Type* ty = mc_expr_rel();
    
    while (cc->tok == TK_EQ || cc->tok == TK_NE) {
        int op = cc->tok;
        mc_next();
        
        mc_thumb_push(1 << mc_reg, 0);
        mc_reg++;
        mc_expr_rel();
        mc_reg--;
        mc_thumb_pop(1 << (mc_reg + 1), 0);
        
        mc_thumb_cmp_reg(mc_reg + 1, mc_reg);
        mc_thumb_mov_imm8(mc_reg, 0);
        mc_thumb_bcc(op == TK_EQ ? CC_NE : CC_EQ, 2);
        mc_thumb_mov_imm8(mc_reg, 1);
        ty = cc->ty_int;
    }
    
    return ty;
}

static Type* mc_expr_and(void) {
    Type* ty = mc_expr_eq();
    
    while (cc->tok == '&') {
        mc_next();
        mc_thumb_push(1 << mc_reg, 0);
        mc_reg++;
        mc_expr_eq();
        mc_reg--;
        mc_thumb_pop(1 << (mc_reg + 1), 0);
        mc_thumb_and_reg(mc_reg, mc_reg + 1);
    }
    
    return ty;
}

static Type* mc_expr_xor(void) {
    Type* ty = mc_expr_and();
    
    while (cc->tok == '^') {
        mc_next();
        mc_thumb_push(1 << mc_reg, 0);
        mc_reg++;
        mc_expr_and();
        mc_reg--;
        mc_thumb_pop(1 << (mc_reg + 1), 0);
        mc_thumb_eor_reg(mc_reg, mc_reg + 1);
    }
    
    return ty;
}

static Type* mc_expr_or(void) {
    Type* ty = mc_expr_xor();
    
    while (cc->tok == '|') {
        mc_next();
        mc_thumb_push(1 << mc_reg, 0);
        mc_reg++;
        mc_expr_xor();
        mc_reg--;
        mc_thumb_pop(1 << (mc_reg + 1), 0);
        mc_thumb_orr_reg(mc_reg, mc_reg + 1);
    }
    
    return ty;
}

static Type* mc_expr_land(void) {
    Type* ty = mc_expr_or();
    
    while (cc->tok == TK_AND) {
        mc_next();
        mc_thumb_cmp_imm8(mc_reg, 0);
        (void)mc_thumb_bcc_placeholder(CC_EQ);  // TODO: patch
        mc_expr_or();
        mc_thumb_cmp_imm8(mc_reg, 0);
        mc_thumb_mov_imm8(mc_reg, 0);
        mc_thumb_bcc(CC_EQ, 2);
        mc_thumb_mov_imm8(mc_reg, 1);
        ty = cc->ty_int;
    }
    
    return ty;
}

static Type* mc_expr_lor(void) {
    Type* ty = mc_expr_land();
    
    while (cc->tok == TK_OR) {
        mc_next();
        mc_thumb_cmp_imm8(mc_reg, 0);
        (void)mc_thumb_bcc_placeholder(CC_NE);  // TODO: patch
        mc_expr_land();
        mc_thumb_cmp_imm8(mc_reg, 0);
        mc_thumb_mov_imm8(mc_reg, 0);
        mc_thumb_bcc(CC_EQ, 2);
        mc_thumb_mov_imm8(mc_reg, 1);
        ty = cc->ty_int;
    }
    
    return ty;
}

static Type* mc_expr_ternary(void) {
    Type* ty = mc_expr_lor();
    
    if (cc->tok == '?') {
        mc_next();
        mc_thumb_cmp_imm8(mc_reg, 0);
        (void)mc_thumb_bcc_placeholder(CC_EQ);  // TODO: patch else_jump
        mc_expr();
        (void)mc_thumb_b_placeholder();  // TODO: patch end_jump
        mc_expect(':');
        mc_expr_ternary();
    }
    
    return ty;
}

static Type* mc_expr_assign(void) {
    Type* ty = mc_expr_ternary();
    
    if (cc->tok == '=' || 
        (cc->tok >= TK_ADD_EQ && cc->tok <= TK_SHR_EQ)) {
        int op = cc->tok;
        mc_next();
        
        // TODO: proper lvalue handling
        mc_expr_assign();
        
        // For compound assignment, apply operation
        if (op != '=') {
            // TODO: implement compound assignment
        }
    }
    
    return ty;
}

static Type* mc_expr(void) {
    Type* ty = mc_expr_assign();
    
    while (cc->tok == ',') {
        mc_next();
        ty = mc_expr_assign();
    }
    
    return ty;
}

// ============================================================================
// STATEMENT CODEGEN
// ============================================================================

static void mc_stmt(void);

static void mc_stmt_block(void) {
    mc_expect('{');
    mc_scope_enter();
    
    while (cc->tok != '}' && cc->tok != TK_EOF) {
        mc_stmt();
    }
    
    mc_scope_leave();
    mc_expect('}');
}

static void mc_stmt_if(void) {
    mc_next();  // Skip 'if'
    mc_expect('(');
    mc_expr();
    mc_expect(')');
    
    mc_thumb_cmp_imm8(mc_reg, 0);
    (void)mc_thumb_bcc_placeholder(CC_EQ);  // TODO: patch else_jump
    
    mc_stmt();
    
    if (cc->tok == TK_ELSE) {
        (void)mc_thumb_b_placeholder();  // TODO: patch end_jump
        mc_next();
        mc_stmt();
    }
}

static void mc_stmt_while(void) {
    mc_next();  // Skip 'while'
    
    uint32_t loop_start = cc->code_pos;
    
    mc_expect('(');
    mc_expr();
    mc_expect(')');
    
    mc_thumb_cmp_imm8(mc_reg, 0);
    (void)mc_thumb_bcc_placeholder(CC_EQ);  // TODO: patch exit_jump
    
    // Push break/continue targets
    int old_break = cc->break_count;
    int old_cont = cc->cont_count;
    
    mc_stmt();
    
    mc_thumb_b(loop_start - cc->code_pos - 4);
    
    // Pop targets and patch breaks
    cc->break_count = old_break;
    cc->cont_count = old_cont;
}

static void mc_stmt_for(void) {
    mc_next();  // Skip 'for'
    mc_expect('(');
    
    mc_scope_enter();
    
    // Init
    if (cc->tok != ';') mc_expr();
    mc_expect(';');
    
    uint32_t cond_start = cc->code_pos;
    
    // Condition
    if (cc->tok != ';') {
        mc_expr();
        mc_thumb_cmp_imm8(mc_reg, 0);
        (void)mc_thumb_bcc_placeholder(CC_EQ);  // TODO: patch exit_jump
    }
    mc_expect(';');
    
    // Skip increment for now (TODO: proper implementation)
    int depth = 1;
    while (depth > 0 && cc->tok != TK_EOF) {
        if (cc->tok == '(') depth++;
        else if (cc->tok == ')') depth--;
        if (depth > 0) mc_next();
    }
    mc_expect(')');
    
    // Body
    mc_stmt();
    
    // Jump back to condition
    mc_thumb_b(cond_start - cc->code_pos - 4);
    
    mc_scope_leave();
}

static void mc_stmt_return(void) {
    mc_next();  // Skip 'return'
    
    if (cc->tok != ';') {
        mc_reg = 0;  // Return value in r0
        mc_expr();
    }
    
    // Epilogue will be added at function end
    mc_thumb_pop(0, 1);  // POP {pc}
    
    mc_expect(';');
}

static void mc_stmt(void) {
    if (cc->had_error) return;
    
    if (cc->tok == '{') {
        mc_stmt_block();
    }
    else if (cc->tok == TK_IF) {
        mc_stmt_if();
    }
    else if (cc->tok == TK_WHILE) {
        mc_stmt_while();
    }
    else if (cc->tok == TK_FOR) {
        mc_stmt_for();
    }
    else if (cc->tok == TK_DO) {
        mc_next();
        uint32_t loop_start = cc->code_pos;
        mc_stmt();
        mc_expect(TK_WHILE);
        mc_expect('(');
        mc_expr();
        mc_expect(')');
        mc_thumb_cmp_imm8(mc_reg, 0);
        mc_thumb_bcc(CC_NE, loop_start - cc->code_pos - 4);
        mc_expect(';');
    }
    else if (cc->tok == TK_RETURN) {
        mc_stmt_return();
    }
    else if (cc->tok == TK_BREAK) {
        mc_next();
        // TODO: emit jump to break target
        mc_expect(';');
    }
    else if (cc->tok == TK_CONTINUE) {
        mc_next();
        // TODO: emit jump to continue target
        mc_expect(';');
    }
    else if (cc->tok == ';') {
        mc_next();  // Empty statement
    }
    else if (cc->tok == TK_INT || cc->tok == TK_CHAR || cc->tok == TK_VOID ||
             cc->tok == TK_SHORT || cc->tok == TK_LONG || cc->tok == TK_UNSIGNED ||
             cc->tok == TK_SIGNED || cc->tok == TK_STATIC || cc->tok == TK_CONST) {
        // Local variable declaration
        mc_next();  // Type
        while (cc->tok == '*' || cc->tok == TK_CONST) mc_next();
        
        if (cc->tok == TK_IDENT) {
            char name[32];
            strncpy(name, cc->tok_str, 31);
            mc_next();
            
            // Add local
            Symbol* sym = mc_sym_add(name, SYM_LOCAL, cc->ty_int);
            cc->local_offset -= 4;
            sym->offset = cc->local_offset;
            
            // Initialize
            if (cc->tok == '=') {
                mc_next();
                mc_expr();
                mc_thumb_str_sp(mc_reg, -cc->local_offset);
            }
            
            while (cc->tok == ',') {
                mc_next();
                while (cc->tok == '*') mc_next();
                if (cc->tok == TK_IDENT) {
                    strncpy(name, cc->tok_str, 31);
                    mc_next();
                    sym = mc_sym_add(name, SYM_LOCAL, cc->ty_int);
                    cc->local_offset -= 4;
                    sym->offset = cc->local_offset;
                    
                    if (cc->tok == '=') {
                        mc_next();
                        mc_expr();
                        mc_thumb_str_sp(mc_reg, -cc->local_offset);
                    }
                }
            }
        }
        mc_expect(';');
    }
    else {
        mc_expr();
        mc_expect(';');
    }
}

// ============================================================================
// FUNCTION CODEGEN
// ============================================================================

static void mc_function(const char* name, Type* ret_type) {
    mc_scope_enter();
    cc->local_offset = 0;
    
    Symbol* func = mc_sym_add(name, SYM_FUNC, ret_type);
    func->offset = cc->code_pos;  // Function address
    
    // Parse parameters
    mc_expect('(');
    int param_count = 0;
    while (cc->tok != ')' && cc->tok != TK_EOF) {
        // Parse type
        while (cc->tok == TK_INT || cc->tok == TK_CHAR || cc->tok == TK_VOID ||
               cc->tok == TK_CONST || cc->tok == '*') {
            mc_next();
        }
        
        if (cc->tok == TK_IDENT) {
            Symbol* param = mc_sym_add(cc->tok_str, SYM_PARAM, cc->ty_int);
            param->offset = param_count * 4;  // Params on stack
            param_count++;
            mc_next();
        }
        
        if (cc->tok == ',') mc_next();
    }
    mc_expect(')');
    
    if (cc->tok == ';') {
        // Function declaration only
        mc_next();
        mc_scope_leave();
        return;
    }
    
    // Function body
    printf("[CC] Compiling function: %s\n", name);
    
    // Prologue: PUSH {lr}
    mc_thumb_push(0, 1);
    
    // Reserve stack space (TODO: patch later)
    (void)cc->code_pos;  // stack_patch position
    mc_thumb_sub_sp_imm(0);  // Placeholder
    
    // Compile body
    mc_expect('{');
    while (cc->tok != '}' && cc->tok != TK_EOF) {
        mc_stmt();
    }
    mc_expect('}');
    
    // Epilogue
    if (cc->local_offset < 0) {
        mc_thumb_add_sp_imm(-cc->local_offset);
    }
    mc_thumb_pop(0, 1);  // POP {pc}
    
    mc_scope_leave();
}

// ============================================================================
// TOP-LEVEL PARSING
// ============================================================================

static void mc_global_decl(void) {
    // Storage class (parsed but not fully used yet)
    if (cc->tok == TK_STATIC) mc_next();
    if (cc->tok == TK_EXTERN) mc_next();
    
    // Type specifier
    Type* base_type = cc->ty_int;
    
    if (cc->tok == TK_VOID) { base_type = cc->ty_void; mc_next(); }
    else if (cc->tok == TK_CHAR) { base_type = cc->ty_char; mc_next(); }
    else if (cc->tok == TK_SHORT) { base_type = cc->ty_int; mc_next(); }  // Treat as int
    else if (cc->tok == TK_INT) { base_type = cc->ty_int; mc_next(); }
    else if (cc->tok == TK_LONG) { base_type = cc->ty_long; mc_next(); }
    else if (cc->tok == TK_UNSIGNED) { 
        mc_next();
        if (cc->tok == TK_INT || cc->tok == TK_CHAR || cc->tok == TK_LONG) mc_next();
        base_type = cc->ty_int;
    }
    else if (cc->tok == TK_SIGNED) {
        mc_next();
        if (cc->tok == TK_INT || cc->tok == TK_CHAR || cc->tok == TK_LONG) mc_next();
        base_type = cc->ty_int;
    }
    else if (cc->tok == TK_STRUCT || cc->tok == TK_UNION) {
        mc_next();
        if (cc->tok == TK_IDENT) mc_next();
        if (cc->tok == '{') {
            // Skip struct definition
            int depth = 1;
            mc_next();
            while (depth > 0 && cc->tok != TK_EOF) {
                if (cc->tok == '{') depth++;
                else if (cc->tok == '}') depth--;
                mc_next();
            }
        }
        base_type = cc->ty_int;  // TODO: proper struct type
    }
    
    // Pointer
    Type* type = base_type;
    while (cc->tok == '*') {
        type = mc_type_ptr(type);
        mc_next();
    }
    
    // Name
    if (cc->tok != TK_IDENT) {
        mc_error("Expected identifier");
        return;
    }
    
    char name[32];
    strncpy(name, cc->tok_str, 31);
    name[31] = 0;
    mc_next();
    
    // Function or variable
    if (cc->tok == '(') {
        // Function
        Type* func_type = mc_type_new(TY_FUNC, 4, 4);
        func_type->base = type;  // Return type
        mc_function(name, func_type);
    } else {
        // Global variable
        Symbol* sym = mc_sym_add(name, SYM_VAR, type);
        sym->offset = cc->bss_pos;
        cc->bss_pos += mc_type_size(type);
        
        if (cc->tok == '=') {
            mc_next();
            // TODO: handle initializer
            mc_expr();
        }
        
        while (cc->tok == ',') {
            mc_next();
            type = base_type;
            while (cc->tok == '*') { type = mc_type_ptr(type); mc_next(); }
            if (cc->tok == TK_IDENT) {
                sym = mc_sym_add(cc->tok_str, SYM_VAR, type);
                sym->offset = cc->bss_pos;
                cc->bss_pos += mc_type_size(type);
                mc_next();
                if (cc->tok == '=') {
                    mc_next();
                    mc_expr();
                }
            }
        }
        mc_expect(';');
    }
}

static void mc_translation_unit(void) {
    while (cc->tok != TK_EOF && !cc->had_error) {
        mc_global_decl();
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

int mimic_compile(const char* input_path, const char* output_path) {
    // Allocate compiler state
    static Compiler compiler;
    cc = &compiler;
    memset(cc, 0, sizeof(Compiler));
    
    // Allocate buffers
    cc->in_buf = mimic_kmalloc(MC_INPUT_BUF);
    cc->out_buf = mimic_kmalloc(MC_OUTPUT_BUF);
    
    if (!cc->in_buf || !cc->out_buf) {
        printf("[CC] Out of memory\n");
        if (cc->in_buf) mimic_kfree(cc->in_buf);
        if (cc->out_buf) mimic_kfree(cc->out_buf);
        return MIMIC_ERR_NOMEM;
    }
    
    // Initialize types
    cc->ty_void = mc_type_new(TY_VOID, 0, 1);
    cc->ty_char = mc_type_new(TY_CHAR, 1, 1);
    cc->ty_int = mc_type_new(TY_INT, 4, 4);
    cc->ty_long = mc_type_new(TY_LONG, 4, 4);
    
    // Open files
    cc->in_fd = mimic_fopen(input_path, MIMIC_FILE_READ);
    if (cc->in_fd < 0) {
        printf("[CC] Cannot open input: %s\n", input_path);
        mimic_kfree(cc->in_buf);
        mimic_kfree(cc->out_buf);
        return MIMIC_ERR_NOENT;
    }
    
    cc->out_fd = mimic_fopen(output_path, MIMIC_FILE_WRITE | MIMIC_FILE_CREATE);
    if (cc->out_fd < 0) {
        printf("[CC] Cannot create output: %s\n", output_path);
        mimic_fclose(cc->in_fd);
        mimic_kfree(cc->in_buf);
        mimic_kfree(cc->out_buf);
        return MIMIC_ERR_IO;
    }
    
    // Initialize state
    cc->line = 1;
    cc->col = 0;
    cc->scope = 0;
    cc->ch = mc_getc();
    
    // Write placeholder header (will be updated later)
    MimiHeader header;
    memset(&header, 0, sizeof(header));
    header.magic = MIMI_MAGIC;
    header.version = MIMI_VERSION;
    header.arch = MIMI_ARCH_THUMB;
    mc_flush();
    mimic_fwrite(cc->out_fd, &header, sizeof(header));
    cc->code_pos = sizeof(header);
    
    // Parse and compile
    printf("[CC] Compiling %s...\n", input_path);
    mc_next();  // Get first token
    mc_translation_unit();
    
    // Flush output
    mc_flush();
    
    // Update header
    header.entry_offset = sizeof(header);  // First function
    header.text_size = cc->code_pos - sizeof(header);
    header.rodata_size = 0;
    header.data_size = 0;
    header.bss_size = cc->bss_pos;
    
    mimic_fseek(cc->out_fd, 0, MIMIC_SEEK_SET);
    mimic_fwrite(cc->out_fd, &header, sizeof(header));
    
    // Cleanup
    mimic_fclose(cc->in_fd);
    mimic_fclose(cc->out_fd);
    mimic_kfree(cc->in_buf);
    mimic_kfree(cc->out_buf);
    
    if (cc->had_error) {
        printf("[CC] Compilation failed: %s (line %lu)\n", cc->error, (unsigned long)cc->error_line);
        return MIMIC_ERR_CORRUPT;
    }
    
    printf("[CC] Success: %lu tokens, %lu bytes code\n", 
           (unsigned long)cc->tokens, (unsigned long)cc->bytes_out);
    return MIMIC_OK;
}

const char* mimic_compile_error(void) {
    return cc ? cc->error : "No compiler state";
}
