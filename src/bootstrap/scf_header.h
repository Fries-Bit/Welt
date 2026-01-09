#ifndef SCF_HEADER_H
#define SCF_HEADER_H

#include "../standard/vtypes.h"

/*
 * Structs, Classes, Functions (SCF)
 */

typedef struct {
    char name[64];
    int arg_count;
    char args[10][64];
    char arg_types[10][64];
    char** body_lines;
    int body_line_count;
    int body_capacity;
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

WeltFunction function_table[100];
int function_count = 0;

void register_global_extension(const char* name) {
    strcpy(global_extensions[g_ext_count].ext_name, name);
    g_ext_count++;
}

WeltFunction* create_function(const char* name) {
    if (function_count >= 100) return NULL;
    
    WeltFunction* f = &function_table[function_count];
    strcpy(f->name, name);
    f->arg_count = 0;
    f->body_lines = NULL;
    f->body_line_count = 0;
    f->body_capacity = 0;
    
    function_count++;
    return f;
}

WeltFunction* get_function(const char* name) {
    for (int i = 0; i < function_count; i++) {
        if (strcmp(function_table[i].name, name) == 0) {
            return &function_table[i];
        }
    }
    return NULL;
}

void add_function_body_line(WeltFunction* f, const char* line) {
    if (f->body_line_count >= f->body_capacity) {
        f->body_capacity = f->body_capacity == 0 ? 10 : f->body_capacity * 2;
        f->body_lines = realloc(f->body_lines, f->body_capacity * sizeof(char*));
    }
    f->body_lines[f->body_line_count] = strdup(line);
    f->body_line_count++;
}

// Logic to dispatch: varname.gname()
WeltValue* call_extension(WeltValue* parent, const char* ext_name) {
    // 1. Find extension
    // 2. Inject 'parent' as implicit context
    // 3. Execute body
    
    
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