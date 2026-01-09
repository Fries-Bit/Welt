#ifndef VTYPES_H
#define VTYPES_H

#include <stdlib.h>
#include <string.h>
#include "../integerbits/integerbits.h"

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
    v->is_hex_repr = 0;
    v->data.i_val = 0;
    v->collection_items = NULL;
    v->collection_size = 0;
    v->collection_cap = 0;
    v->bit_info.bits = 32;
    v->bit_info.is_unsigned = 0;
    v->bit_info.is_arch_dep = 0;
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