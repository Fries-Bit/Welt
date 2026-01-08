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