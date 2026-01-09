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
    *dest = '\0';
}

// The Print Function: print("Hello {}@", var);
void welt_print(const char* fmt, WeltValue** args, int arg_count) {
    if (!fmt) {
        printf("\n");
        return;
    }
    
    const char* p = fmt;
    int arg_index = 0;
    while (*p) {
        if (*p == '{' && *(p+1) == '}' && *(p+2) == '@') {
            if (arg_index < arg_count && args[arg_index]) {
                WeltValue* arg = args[arg_index];
                if (arg) {
                    switch(arg->type) {
                        case WT_INTEGER: 
                            if(arg->is_hex_repr) printf("%llX", arg->data.i_val);
                            else printf("%lld", arg->data.i_val); 
                            break;
                        case WT_STRING: 
                            if(arg->data.s_val) printf("%s", arg->data.s_val);
                            else printf("(null)");
                            break;
                        case WT_BOOL: printf(arg->data.b_val ? "true" : "false"); break;
                        case WT_FLOAT: printf("%f", arg->data.f_val); break;
                        case WT_SYS_IND: printf("<sys_ind:%p>", arg->data.ptr_val); break;
                        default: printf("nil");
                    }
                } else {
                    printf("(null)");
                }
            } else {
                printf("(missing)");
            }
            arg_index++;
            p += 3;
        } else {
            putchar(*p);
            p++;
        }
    }
    putchar('\n');
}

// cc_input("hello");
WeltValue* cc_input(const char* prompt) {
    printf("%s", prompt);
    char buffer[1024];
    if (fgets(buffer, 1024, stdin)) {
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline
        WeltValue* v = wv_create();
        v->type = WT_STRING;
        v->data.s_val = strdup(buffer);
        return v;
    }
    return NULL;
}

int loose_equals(WeltValue* a, WeltValue* b) {
    if (!a || !b) return 0;
    
    char bufA[256], bufB[256];
    char cleanA[256], cleanB[256];
    
    bufA[0] = '\0';
    bufB[0] = '\0';
    
    if(a->type == WT_INTEGER) {
        snprintf(bufA, sizeof(bufA), "%lld", a->data.i_val);
    } else if(a->type == WT_STRING && a->data.s_val) {
        snprintf(bufA, sizeof(bufA), "%s", a->data.s_val);
    } else {
        strcpy(bufA, "nil");
    }
    
    if(b->type == WT_INTEGER) {
        snprintf(bufB, sizeof(bufB), "%lld", b->data.i_val);
    } else if(b->type == WT_STRING && b->data.s_val) {
        snprintf(bufB, sizeof(bufB), "%s", b->data.s_val);
    } else {
        strcpy(bufB, "nil");
    }

    strip_commas(cleanA, bufA);
    strip_commas(cleanB, bufB);

    return (strcasecmp(cleanA, cleanB) == 0);
}

int strict_equals(WeltValue* a, WeltValue* b) {
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;
    
    switch(a->type) {
        case WT_INTEGER:
            return a->data.i_val == b->data.i_val;
        case WT_FLOAT:
            return a->data.f_val == b->data.f_val;
        case WT_BOOL:
            return a->data.b_val == b->data.b_val;
        case WT_STRING:
            if (!a->data.s_val || !b->data.s_val) return 0;
            return strcmp(a->data.s_val, b->data.s_val) == 0;
        case WT_SYS_IND:
            return a->data.ptr_val == b->data.ptr_val;
        default:
            return 0;
    }
}

#endif