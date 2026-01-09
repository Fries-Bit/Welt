#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_SUGGESTIONS 3

typedef struct {
    const char* keyword;
    const char* description;
} WeltKeyword;

static const WeltKeyword WELT_KEYWORDS[] = {
    {"integer", "integer type"},
    {"string", "string type"},
    {"float", "floating-point type"},
    {"bool", "boolean type"},
    {"ss", "auto-detect type"},
    {"const", "constant modifier"},
    {"print", "print function"},
    {"sa_free", "memory free function"},
    {"fwww_at", "network fetch function"},
    {"cc_input", "input function"},
    {"if", "conditional statement"},
    {"else", "else clause"},
    {"while", "while loop"},
    {"for", "for loop"},
    {"fn", "function declaration"},
    {"main", "main entry point function"},
    {"return", "return statement"},
    {"structure", "structure declaration"},
    {"class", "class declaration"},
    {"global_extentions", "global extensions"},
    {NULL, NULL}
};

int levenshtein_distance(const char* s1, const char* s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;
    
    int matrix[len1 + 1][len2 + 1];
    
    for (int i = 0; i <= len1; i++) matrix[i][0] = i;
    for (int j = 0; j <= len2; j++) matrix[0][j] = j;
    
    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (tolower(s1[i-1]) == tolower(s2[j-1])) ? 0 : 1;
            
            int deletion = matrix[i-1][j] + 1;
            int insertion = matrix[i][j-1] + 1;
            int substitution = matrix[i-1][j-1] + cost;
            
            matrix[i][j] = deletion;
            if (insertion < matrix[i][j]) matrix[i][j] = insertion;
            if (substitution < matrix[i][j]) matrix[i][j] = substitution;
        }
    }
    
    return matrix[len1][len2];
}

void welt_error_unknown_keyword(const char* unknown, int line_num) {
    printf("\033[1;31merror\033[0m: unknown keyword '\033[1m%s\033[0m'\n", unknown);
    printf("  \033[1;34m-->\033[0m line %d\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("%2d \033[1;34m|\033[0m  %s\n", line_num, unknown);
    printf("   \033[1;34m|\033[0m  \033[1;31m");
    for (size_t i = 0; i < strlen(unknown); i++) printf("^");
    printf("\033[0m\n");
    
    int best_distance = 999;
    const char* suggestions[MAX_SUGGESTIONS];
    int suggestion_count = 0;
    
    for (int i = 0; WELT_KEYWORDS[i].keyword != NULL; i++) {
        int dist = levenshtein_distance(unknown, WELT_KEYWORDS[i].keyword);
        
        if (dist <= 2 && dist < best_distance) {
            best_distance = dist;
            suggestion_count = 0;
        }
        
        if (dist == best_distance && suggestion_count < MAX_SUGGESTIONS) {
            suggestions[suggestion_count++] = WELT_KEYWORDS[i].keyword;
        }
    }
    
    if (suggestion_count > 0) {
        printf("   \033[1;34m|\033[0m\n");
        if (suggestion_count == 1) {
            printf("   \033[1;34m= \033[1mhelp\033[0m: did you mean '\033[1;32m%s\033[0m'?\n", suggestions[0]);
        } else {
            printf("   \033[1;34m= \033[1mhelp\033[0m: did you mean one of these?\n");
            for (int i = 0; i < suggestion_count; i++) {
                printf("           '\033[1;32m%s\033[0m'\n", suggestions[i]);
            }
        }
    }
    printf("\n");
}

void welt_error_type_mismatch(const char* expected, const char* got, int line_num) {
    printf("\033[1;31merror\033[0m: type mismatch\n");
    printf("  \033[1;34m-->\033[0m line %d\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("%2d \033[1;34m|\033[0m  expected '\033[1;32m%s\033[0m', got '\033[1;31m%s\033[0m'\n", 
           line_num, expected, got);
    printf("\n");
}

void welt_error_const_reassignment(const char* var_name, int line_num) {
    printf("\033[1;31merror[E0384]\033[0m: cannot assign twice to immutable variable '\033[1m%s\033[0m'\n", var_name);
    printf("  \033[1;34m-->\033[0m line %d\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("%2d \033[1;34m|\033[0m  cannot assign twice to immutable variable\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("   \033[1;34m= \033[1mhelp\033[0m: consider making this variable mutable: '\033[1;32m%s\033[0m'\n", var_name);
    printf("\n");
}

void welt_error_undefined_variable(const char* var_name, int line_num) {
    printf("\033[1;31merror[E0425]\033[0m: cannot find value '\033[1m%s\033[0m' in this scope\n", var_name);
    printf("  \033[1;34m-->\033[0m line %d\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("%2d \033[1;34m|\033[0m  not found in this scope\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("   \033[1;34m= \033[1mhelp\033[0m: variable '\033[1m%s\033[0m' has not been declared\n", var_name);
    printf("\n");
}

void welt_error_max_vars(int max, int line_num) {
    printf("\033[1;31merror\033[0m: symbol table overflow\n");
    printf("  \033[1;34m-->\033[0m line %d\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("%2d \033[1;34m|\033[0m  exceeded maximum number of variables (%d)\n", line_num, max);
    printf("   \033[1;34m|\033[0m\n");
    printf("   \033[1;34m= \033[1mhelp\033[0m: consider using functions or reducing variable count\n");
    printf("\n");
}

void welt_warning_unused_variable(const char* var_name, int line_num) {
    printf("\033[1;33mwarning\033[0m: unused variable: '\033[1m%s\033[0m'\n", var_name);
    printf("  \033[1;34m-->\033[0m line %d\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("%2d \033[1;34m|\033[0m  unused variable\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("   \033[1;34m= \033[1mnote\033[0m: '\033[1m#[allow(unused_variables)]\033[0m' on by default\n");
    printf("\n");
}

void welt_error_syntax(const char* message, const char* snippet, int line_num) {
    printf("\033[1;31merror\033[0m: syntax error\n");
    printf("  \033[1;34m-->\033[0m line %d\n", line_num);
    printf("   \033[1;34m|\033[0m\n");
    printf("%2d \033[1;34m|\033[0m  %s\n", line_num, snippet);
    printf("   \033[1;34m|\033[0m  \033[1;31m");
    for (size_t i = 0; i < strlen(snippet); i++) printf("^");
    printf("\033[0m %s\n", message);
    printf("\n");
}

void welt_info(const char* message) {
    printf("\033[1;36minfo\033[0m: %s\n\n", message);
}

#endif
