#ifndef SCF_HEADER_H
#define SCF_HEADER_H

#include "../standard/vtypes.h"

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