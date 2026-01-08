#ifndef IS_HEADER_H
#define IS_HEADER_H

#include "../standard/vtypes.h"
#include "../standard/basic.h"
#include <ctype.h>

/*
 * Updated Welt IF Statement System
 * Now supports:
 * - integer matching (10 -> { ... })
 * - both {} and [] for arrow blocks
 */

typedef struct {
    char condition_raw[256];
    int is_arrow_syntax; 
} IfStatement;

void execute_arrow_block(char* block_content, WeltValue* condition_val) {
    // We iterate through potential match lines (pattern -> [code] or pattern -> {code})
    char* line = strtok(block_content, "\n,"); // Split by newline or comma
    while(line) {
        char* arrow = strstr(line, "->");
        if(arrow) {
            *arrow = '\0';
            char* lhs = line;      // The pattern (e.g., "isthat" or 10)
            char* rhs = arrow + 2; // The code block
            
            // Trim leading whitespace on LHS
            while(isspace(*lhs)) lhs++;
            
            int match = 0;
            
            // 1. Boolean Match
            if(strcmp(lhs, "true") == 0 && condition_val->data.b_val) match = 1;
            else if(strcmp(lhs, "false") == 0 && !condition_val->data.b_val) match = 1;
            
            // 2. Integer Match
            else if(isdigit(*lhs) || (*lhs == '-' && isdigit(*(lhs+1)))) {
                long long pattern_val = atoll(lhs);
                if(condition_val->type == WT_INTEGER && condition_val->data.i_val == pattern_val) {
                    match = 1;
                }
            }
            
            // 3. String Match
            else {
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
                // Check for either bracket style: [ code ] or { code }
                char* b_start = strpbrk(rhs, "[{");
                if(b_start) {
                    char bracket_type = *b_start;
                    printf("DEBUG: Executing Arrow Block (%c) for pattern: %s\n", bracket_type, lhs);
                    // In the full interpreter, you would send the text between 
                    // b_start and its matching closing bracket back to the main loop.
                }
            }
        }
        line = strtok(NULL, "\n,");
    }
}

#endif