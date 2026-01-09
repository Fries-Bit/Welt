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

unsigned long long mask_bits(unsigned long long val, int bits) {
    if (bits <= 0) return 0;
    if (bits >= 64) return val;
    return val & ((1ULL << bits) - 1);
}

long long parse_hex_literal(const char* token) {
    if (!token || strstr(token, "<hex>") != token) {
        return 0;
    }
    
    const char* hex_start = token + 5;
    char *endptr;
    long long result = strtoll(hex_start, &endptr, 16);
    
    if (endptr == hex_start) {
        printf("Warning: Invalid hex literal '%s'\n", token);
        return 0;
    }
    
    return result;
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
        bc.var_name[name_len] = '\0';
        
        // Parse internals
        char inner[32];
        int len = end - start - 1;
        if(len > 31) len = 31;
        strncpy(inner, start + 1, len);
        inner[len] = '\0';
        
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