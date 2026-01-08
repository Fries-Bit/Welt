import os

# --- Configuration ---
ROOT_DIR = "welt_lang"
SRC_DIR = os.path.join(ROOT_DIR, "src")
DIRS = {
    "standard": os.path.join(SRC_DIR, "standard"),
    "bootstrap": os.path.join(SRC_DIR, "bootstrap"),
    "integerbits": os.path.join(SRC_DIR, "integerbits"),
}

# --- Content Generators ---

def get_integerbits_h():
    return """
#ifndef INTEGERBITS_H
#define INTEGERBITS_H

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* * Welt Integer Bits System
 * Handles <i(number)bit>, <u(number)bit>, <hex>
 */

typedef struct {
    int is_signed;
    int bit_width; // 0 means architecture dependent (i*bit)
    unsigned long long u_val;
    long long i_val;
    int is_hex;
} WeltBitInt;

// Determine architecture dependent bit width
#define ARCH_BIT (sizeof(void*) * 8)

// Helper to mask values to specific bit sizes
unsigned long long mask_bits(unsigned long long val, int bits) {
    if (bits >= 64 || bits <= 0) return val;
    return val & ((1ULL << bits) - 1);
}

// Parse <hex>1234
long long parse_hex_literal(const char* token) {
    // Expected format: <hex>123...
    if (strstr(token, "<hex>") == token) {
        char *ptr;
        return strtoll(token + 5, &ptr, 16);
    }
    return 0;
}

// Logic for declaring: vartype varname<i(bitsize)>
// This struct holds metadata about the bit constraints
typedef struct {
    char var_name[64];
    int bits;
    int is_unsigned;
    int is_arch_dep; // 1 if *bit
} BitConstraint;

BitConstraint parse_bit_def(const char* def_str) {
    BitConstraint bc;
    bc.bits = 32; // default
    bc.is_unsigned = 0;
    bc.is_arch_dep = 0;
    
    // Example: varname<i8bit> or varname<u*bit>
    char *start = strchr(def_str, '<');
    char *end = strchr(def_str, '>');
    
    if (start && end) {
        // Extract name
        int name_len = start - def_str;
        strncpy(bc.var_name, def_str, name_len);
        bc.var_name[name_len] = '\\0';
        
        // Parse internals
        char inner[32];
        int len = end - start - 1;
        if(len > 31) len = 31;
        strncpy(inner, start + 1, len);
        inner[len] = '\\0';
        
        // check signed/unsigned
        if (inner[0] == 'u') bc.is_unsigned = 1;
        
        // check width
        if (strchr(inner, '*')) {
            bc.is_arch_dep = 1;
            bc.bits = ARCH_BIT;
        } else {
            // Extract number i(256)bit -> 256
            // Simple parsing assuming format letter(number)bit
            int i = 1; 
            int num = 0;
            while(inner[i] >= '0' && inner[i] <= '9') {
                num = num * 10 + (inner[i] - '0');
                i++;
            }
            if(num > 0) bc.bits = num;
        }
    } else {
        strcpy(bc.var_name, def_str);
    }
    return bc;
}

#endif
"""

def get_vtypes_h():
    return """
#ifndef VTYPES_H
#define VTYPES_H

#include <stdlib.h>
#include <string.h>
#include "integerbits.h"

/*
 * Welt Variable Type System
 * Supports: integer, string, bool, float, ss (auto), const, sys_ind
 */

typedef enum {
    WT_SS_NULL, // Uninitialized
    WT_INTEGER,
    WT_FLOAT,
    WT_STRING,
    WT_BOOL,
    WT_SYS_IND, // System indicator / pointer
    WT_TABLE,   // []
    WT_ARRAY    // {}
} WeltType;

typedef struct WeltValue WeltValue;

struct WeltValue {
    WeltType type;
    
    // Values union
    union {
        long long i_val;
        double f_val;
        char* s_val;
        int b_val;
        void* ptr_val; // For sys_ind or raw address
    } data;

    // Metadata
    BitConstraint bit_info;
    int is_const;
    int is_hex_repr;
    
    // Collection support
    WeltValue** collection_items;
    int collection_size;
    int collection_cap;
};

// Constructor
WeltValue* wv_create() {
    WeltValue* v = (WeltValue*)malloc(sizeof(WeltValue));
    v->type = WT_SS_NULL;
    v->is_const = 0;
    v->data.i_val = 0;
    v->collection_items = NULL;
    v->collection_size = 0;
    return v;
}

// Memory Management
void sa_free(WeltValue* v) {
    if (!v) return;
    if (v->type == WT_STRING && v->data.s_val) {
        free(v->data.s_val);
    }
    if (v->collection_items) {
        for(int i=0; i<v->collection_size; i++) {
            sa_free(v->collection_items[i]);
        }
        free(v->collection_items);
    }
    free(v);
}

// Address fetching: varname.addr
char* wv_get_addr_str(WeltValue* v) {
    char* buf = malloc(64);
    sprintf(buf, "%p", (void*)v);
    return buf;
}

// Length: varname.length_of_variable()
int wv_length(WeltValue* v) {
    if(v->type == WT_STRING) return strlen(v->data.s_val);
    if(v->type == WT_TABLE || v->type == WT_ARRAY) return v->collection_size;
    return sizeof(v->data); // fallback for primitives
}

#endif
"""

def get_basic_h():
    return """
#ifndef BASIC_H
#define BASIC_H

#include <stdio.h>
#include <ctype.h>
#include "vtypes.h"

/*
 * Basic Operations
 * print, input, string formatting
 */

// Helper: Remove commas from integer string for == comparison
// "1,000" -> "1000"
void strip_commas(char* dest, const char* src) {
    while (*src) {
        if (*src != ',') {
            *dest = *src;
            dest++;
        }
        src++;
    }
    *dest = '\\0';
}

// The Print Function: print("Hello {}@", var);
void welt_print(const char* fmt, WeltValue* arg) {
    const char* p = fmt;
    while (*p) {
        if (*p == '{' && *(p+1) == '}' && *(p+2) == '@') {
            // Injection point
            if (arg) {
                switch(arg->type) {
                    case WT_INTEGER: 
                        if(arg->is_hex_repr) printf("%llX", arg->data.i_val);
                        else printf("%lld", arg->data.i_val); 
                        break;
                    case WT_STRING: printf("%s", arg->data.s_val); break;
                    case WT_BOOL: printf(arg->data.b_val ? "true" : "false"); break;
                    case WT_FLOAT: printf("%f", arg->data.f_val); break;
                    case WT_SYS_IND: printf("<sys_ind:%p>", arg->data.ptr_val); break;
                    default: printf("nil");
                }
            }
            p += 3; // skip {}@
        } else {
            putchar(*p);
            p++;
        }
    }
    putchar('\\n');
}

// cc_input("hello");
WeltValue* cc_input(const char* prompt) {
    printf("%s", prompt);
    char buffer[1024];
    if (fgets(buffer, 1024, stdin)) {
        buffer[strcspn(buffer, "\\n")] = 0; // Remove newline
        WeltValue* v = wv_create();
        v->type = WT_STRING;
        v->data.s_val = strdup(buffer);
        return v;
    }
    return NULL;
}

// The == operator (Not case or integer sensitive, ignores commas)
int loose_equals(WeltValue* a, WeltValue* b) {
    // Conversion to string for loose comparison
    // Implementation simplified for prototype
    char bufA[256], bufB[256];
    char cleanA[256], cleanB[256];
    
    // ... (Populate bufA/B based on type) ...
    // Assuming integers for the example constraint:
    if(a->type == WT_INTEGER) sprintf(bufA, "%lld", a->data.i_val);
    else if(a->type == WT_STRING) strcpy(bufA, a->data.s_val);
    
    if(b->type == WT_INTEGER) sprintf(bufB, "%lld", b->data.i_val);
    else if(b->type == WT_STRING) strcpy(bufB, b->data.s_val);

    strip_commas(cleanA, bufA);
    strip_commas(cleanB, bufB);

    // Case insensitive compare
    return (strcasecmp(cleanA, cleanB) == 0);
}

// The === operator (Strict)
int strict_equals(WeltValue* a, WeltValue* b) {
    if (a->type != b->type) return 0;
    if (a->type == WT_INTEGER) return a->data.i_val == b->data.i_val;
    if (a->type == WT_STRING) return strcmp(a->data.s_val, b->data.s_val) == 0;
    return 0;
}

#endif
"""

def get_is_header_h():
    return """
#ifndef IS_HEADER_H
#define IS_HEADER_H

#include "vtypes.h"
#include "basic.h"

/*
 * IF Statement System
 * Handles:
 * 1. Standard if/else
 * 2. Pattern matching (arrow syntax ->)
 * 3. Bool shorthand (cond == "that" ? ...)
 */

typedef struct {
    char condition_raw[256];
    int is_arrow_syntax; // 1 if contains ->
} IfStatement;

// Simplified parser for the Arrow Syntax body
// Input: isthat -> [ ... ]
void execute_arrow_block(char* block_content, WeltValue* condition_val) {
    // This is a mock parser logic. In a full compiler, this would traverse AST nodes.
    // We look for patterns matching condition_val
    
    char* line = strtok(block_content, "\\n");
    while(line) {
        char* arrow = strstr(line, "->");
        if(arrow) {
            // Split LHS and RHS
            *arrow = '\\0';
            char* lhs = line; 
            char* rhs = arrow + 2;
            
            // Clean whitespace
            while(isspace(*lhs)) lhs++;
            
            int match = 0;
            
            if(strcmp(lhs, "true") == 0 && condition_val->data.b_val) match = 1;
            else if(strcmp(lhs, "false") == 0 && !condition_val->data.b_val) match = 1;
            else {
                 // String match "isthat"
                 // Remove quotes
                 char *q = strchr(lhs, '"');
                 if(q) {
                     char *endq = strrchr(lhs, '"');
                     if(endq) *endq = 0;
                     if(condition_val->type == WT_STRING && strcmp(q+1, condition_val->data.s_val) == 0) {
                         match = 1;
                     }
                 }
            }

            if(match) {
                // Check if bracketed block [ code ]
                char* bracket_start = strchr(rhs, '[');
                if(bracket_start) {
                   // Execute Code Here (Recursively call interpreter)
                   printf("DEBUG: Executing Arrow Match Block for %s\\n", lhs);
                }
            }
        }
        line = strtok(NULL, "\\n");
    }
}

#endif
"""

def get_scf_header_h():
    return """
#ifndef SCF_HEADER_H
#define SCF_HEADER_H

#include "vtypes.h"

/*
 * Structs, Classes, Functions (SCF)
 */

typedef struct {
    char name[64];
    // In a real interpreter, this would store AST nodes for the function body
    long file_offset; 
    int arg_count;
    char args[10][64]; // Up to 10 args for demo
    char arg_types[10][64]; // Types: string, integer<u8bit>, etc
} WeltFunction;

typedef struct {
    char name[64];
    // Fields...
} WeltStructure;

typedef struct {
    char name[64];
    // Methods...
} WeltClass;

// Global Extensions Registry
// global_extentions gname { ... }
typedef struct {
    char ext_name[64];
    // The code body for the extension
    char code_body[1024]; 
} WeltGlobalExt;

WeltGlobalExt global_extensions[50];
int g_ext_count = 0;

void register_global_extension(const char* name) {
    strcpy(global_extensions[g_ext_count].ext_name, name);
    g_ext_count++;
}

// Logic to dispatch: varname.gname()
WeltValue* call_extension(WeltValue* parent, const char* ext_name) {
    // 1. Find extension
    // 2. Inject 'parent' as implicit context
    // 3. Execute body
    
    // Demo logic:
    printf("Calling global extension %s on variable type %d\\n", ext_name, parent->type);
    
    if (strcmp(ext_name, "bytes") == 0) {
        // Built-in behavior requested in prompt
        WeltValue* res = wv_create();
        res->type = WT_INTEGER;
        res->data.i_val = wv_length(parent); // simplified "bytes" logic
        return res;
    }
    
    return NULL;
}

#endif
"""

def get_network_h():
    return """
#ifndef NETWORK_H
#define NETWORK_H

#include "vtypes.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Network Operations
 * fwww_at(url, varname);
 */

void fwww_at(const char* url, WeltValue* output_var) {
    // Since writing a raw HTTP client in C is 1000+ lines,
    // We will use a system call to curl for the interpreter prototype.
    
    printf("Fetching content from: %s\\n", url);
    
    char cmd[512];
    sprintf(cmd, "curl -s \"%s\" > .welt_tmp_net", url);
    int res = system(cmd);
    
    if (res == 0) {
        FILE* f = fopen(".welt_tmp_net", "r");
        if(f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            char *string = malloc(fsize + 1);
            fread(string, 1, fsize, f);
            fclose(f);
            
            string[fsize] = 0;
            
            output_var->type = WT_STRING;
            output_var->data.s_val = string;
            
            // Cleanup
            remove(".welt_tmp_net");
        }
    } else {
        printf("Error: Network request failed.\\n");
    }
}

#endif
"""

def get_main_c():
    return """
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Include Welt Headers
#include "integerbits.h"
#include "vtypes.h"
#include "basic.h"
#include "is_header.h"
#include "scf_header.h"
#include "network.h"

#define MAX_CODE_SIZE 100000
#define MAX_VARS 1000

// --- Symbol Table ---
typedef struct {
    char name[64];
    WeltValue* val;
} Symbol;

Symbol symbol_table[MAX_VARS];
int symbol_count = 0;

WeltValue* get_var(const char* name) {
    for(int i=0; i<symbol_count; i++) {
        if(strcmp(symbol_table[i].name, name) == 0) {
            return symbol_table[i].val;
        }
    }
    return NULL;
}

void set_var(const char* name, WeltValue* val) {
    WeltValue* existing = get_var(name);
    if(existing) {
        // Logic to update existing, respecting const
        if(existing->is_const) {
            printf("Error: Cannot modify const %s\\n", name);
            return;
        }
        // Sa_free old data if complex type...
        *existing = *val; // Shallow copy for prototype
    } else {
        strcpy(symbol_table[symbol_count].name, name);
        symbol_table[symbol_count].val = val;
        symbol_count++;
    }
}

// --- Interpreter Logic ---

void parse_line(char* line) {
    // Remove comments
    char* comment = strstr(line, "//");
    if(comment) *comment = '\\0';
    
    // Trim whitespace
    while(isspace(*line)) line++;
    if(*line == 0) return;

    // 1. Check for includes
    if(strncmp(line, "#include", 8) == 0) {
        printf("Parsing Include: %s\\n", line+8);
        return;
    }

    // 2. Check for print("...")
    if(strncmp(line, "print(", 6) == 0) {
        char* content = line + 6;
        char* end = strrchr(content, ')');
        if(end) *end = 0;
        
        // Check for args
        char* comma = strchr(content, ',');
        if(comma) {
            *comma = 0;
            char* var_name = comma + 1;
            while(isspace(*var_name)) var_name++;
            
            // Handle string literal
            if(content[0] == '"') content++;
            char* closing_quote = strrchr(content, '"');
            if(closing_quote) *closing_quote = 0;
            
            WeltValue* v = get_var(var_name);
            welt_print(content, v);
        } else {
            // Simple string
             if(content[0] == '"') content++;
             char* closing_quote = strrchr(content, '"');
             if(closing_quote) *closing_quote = 0;
             welt_print(content, NULL);
        }
        return;
    }

    // 3. Variable Declaration: type name = value;
    // Simple parser for: integer x = 10;
    // or: ss x = "test";
    char type_buf[32], name_buf[64], val_buf[256];
    if (sscanf(line, "%s %s = %[^;];", type_buf, name_buf, val_buf) == 3) {
        WeltValue* v = wv_create();
        
        // Handle Types
        if(strcmp(type_buf, "integer") == 0) {
            v->type = WT_INTEGER;
            
            // Check for Hex Literal <hex>
            if(strstr(val_buf, "<hex>")) {
                v->data.i_val = parse_hex_literal(val_buf);
                v->is_hex_repr = 1;
            } else {
                v->data.i_val = atoll(val_buf);
            }
        } 
        else if(strcmp(type_buf, "string") == 0) {
            v->type = WT_STRING;
            // strip quotes
            char* start = strchr(val_buf, '"');
            if(start) {
                char* end = strrchr(start+1, '"');
                if(end) *end = 0;
                v->data.s_val = strdup(start+1);
            }
        }
        else if(strcmp(type_buf, "ss") == 0) {
            // Auto detect
            if(strchr(val_buf, '"')) {
                v->type = WT_STRING;
                v->data.s_val = strdup(val_buf); // sloppy quote handling for brevity
            } else {
                v->type = WT_INTEGER;
                v->data.i_val = atoll(val_buf);
            }
        }

        // Handle bit defs in name: varname<i8bit>
        BitConstraint bc = parse_bit_def(name_buf);
        if(strchr(name_buf, '<')) {
             // Clean name
             *strchr(name_buf, '<') = 0;
        }
        v->bit_info = bc;
        
        // Enforce bit constraints
        if(v->type == WT_INTEGER && bc.bits > 0) {
            v->data.i_val = mask_bits(v->data.i_val, bc.bits);
        }

        set_var(name_buf, v);
        return;
    }
    
    // 4. Handle fwww_at
    if(strncmp(line, "fwww_at(", 8) == 0) {
        // Parse args
        // fwww_at("url", varname)
        // ... simplified extraction ...
        printf("Executing network request...\\n");
    }
    
    // 5. Handle sa_free
    if(strncmp(line, "sa_free(", 8) == 0) {
        char* name = line + 8;
        char* end = strchr(name, ')');
        if(end) *end=0;
        WeltValue* v = get_var(name);
        if(v) {
            sa_free(v);
            // Remove from symbol table (simplified: just nullify)
            // In real code, shift array
            printf("Memory freed for %s\\n", name);
        }
    }
}

// --- Main Entry ---

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Welt Interpreter 1.0\\nUsage: welt <file.wt>\\n");
        return 1;
    }

    FILE* fp = fopen(argv[1], "r");
    if (!fp) {
        printf("Error: Could not open file %s\\n", argv[1]);
        return 1;
    }

    char line[1024];
    int in_block_comment = 0;

    // A real interpreter would tokenize the whole file first into an AST.
    // This loop simulates a line-by-line interpreter for basic statements.
    while (fgets(line, sizeof(line), fp)) {
        // Strip newline
        line[strcspn(line, "\\n")] = 0;
        
        if(in_block_comment) {
            if(strstr(line, "*/")) in_block_comment = 0;
            continue;
        }
        if(strstr(line, "/*")) {
            in_block_comment = 1;
            continue;
        }

        parse_line(line);
    }

    fclose(fp);
    return 0;
}
"""

def get_test_wt():
    return """
// Welt Language Test File
#include "std.wt"

/* Test Comment Block
*/

integer myNum = 100;
string myName = "User";

// Testing Hex and SS
ss autoVar = 500;
integer hexVal = <hex>FF; 

// Bit types
integer smallNum<i8bit> = 255;

print("Basic Print Test");
print("Hello {}@", myName);

// Testing memory
sa_free(autoVar);

// Network (Mock)
// fwww_at("http://example.com", htmlRes);
"""

# --- File Writing Logic ---

def write_file(path, content):
    with open(path, "w") as f:
        f.write(content.strip())
    print(f"[Created] {path}")

def main():
    # 1. Create Directories
    if not os.path.exists(ROOT_DIR):
        os.makedirs(ROOT_DIR)
    
    if not os.path.exists(SRC_DIR):
        os.makedirs(SRC_DIR)
        
    for key, path in DIRS.items():
        if not os.path.exists(path):
            os.makedirs(path)

    # 2. Write Headers
    write_file(os.path.join(DIRS["integerbits"], "integerbits.h"), get_integerbits_h())
    write_file(os.path.join(DIRS["standard"], "vtypes.h"), get_vtypes_h())
    write_file(os.path.join(DIRS["standard"], "basic.h"), get_basic_h())
    write_file(os.path.join(DIRS["standard"], "network.h"), get_network_h())
    write_file(os.path.join(DIRS["bootstrap"], "is_header.h"), get_is_header_h())
    write_file(os.path.join(DIRS["bootstrap"], "scf_header.h"), get_scf_header_h())

    # 3. Write Main
    write_file(os.path.join(SRC_DIR, "main.c"), get_main_c())
    
    # 4. Write Test File
    write_file(os.path.join(ROOT_DIR, "test.wt"), get_test_wt())

    print("\\nWelt environment generated successfully.")
    print("To compile (on Linux/Mac/WSL):")
    print("gcc src/main.c -o welt -I src/standard -I src/bootstrap -I src/integerbits")
    print("./welt test.wt")

if __name__ == "__main__":
    main()