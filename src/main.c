#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Include Welt Headers
#include "integerbits/integerbits.h"
#include "standard/vtypes.h"
#include "standard/basic.h"
#include "bootstrap/is_header.h"
#include "bootstrap/scf_header.h"
#include "standard/network.h"

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
            printf("Error: Cannot modify const %s\n", name);
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
    if(comment) *comment = '\0';
    
    // Trim whitespace
    while(isspace(*line)) line++;
    if(*line == 0) return;

    // 1. Check for includes
    if(strncmp(line, "#include", 8) == 0) {
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
        }
    }
}

// --- Main Entry ---

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Welt Interpreter 1.0\nUsage: welt <file.wt>\n");
        return 1;
    }

    FILE* fp = fopen(argv[1], "r");
    if (!fp) {
        printf("Error: Could not open file %s\n", argv[1]);
        return 1;
    }

    char line[1024];
    int in_block_comment = 0;

    // A real interpreter would tokenize the whole file first into an AST.
    // This loop simulates a line-by-line interpreter for basic statements.
    while (fgets(line, sizeof(line), fp)) {
        // Strip newline
        line[strcspn(line, "\n")] = 0;
        
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