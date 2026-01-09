#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Include Welt Headers
#include "integerbits/integerbits.h"
#include "standard/vtypes.h"
#include "standard/basic.h"
#include "standard/errors.h"
#include "bootstrap/is_header.h"
#include "bootstrap/scf_header.h"
#include "standard/network.h"
#include "standard/fsal_bindings.h"

#define MAX_CODE_SIZE 100000
#define MAX_VARS 1000

// --- Symbol Table ---
typedef struct {
    char name[64];
    WeltValue* val;
} Symbol;

Symbol symbol_table[MAX_VARS];
int symbol_count = 0;
int current_line_number = 0;
WeltFunction* current_function = NULL;
int in_function_body = 0;

int in_if_block = 0;
int if_condition_result = 0;
int skip_if_block = 0;

int in_for_loop = 0;
int for_loop_condition = 1;
char for_init[256];
char for_cond[256];
char for_incr[256];
char* for_body_lines[1000];
int for_body_count = 0;

int fsal_imported = 0;
FSALFile* fsal_current_file = NULL;

WeltValue* function_return_value = NULL;
int function_has_returned = 0;

int split_args_respecting_parens(char* args_str, char* results[], int max_results) {
    int count = 0;
    int paren_depth = 0;
    char* start = args_str;
    char* p = args_str;
    
    while (*p && count < max_results) {
        if (*p == '(') {
            paren_depth++;
        } else if (*p == ')') {
            paren_depth--;
        } else if (*p == ',' && paren_depth == 0) {
            *p = '\0';
            results[count++] = start;
            start = p + 1;
        }
        p++;
    }
    
    if (start < p && count < max_results) {
        results[count++] = start;
    }
    
    return count;
}

void welt_print_value(WeltValue* v) {
    if(!v) {
        printf("(null)");
        return;
    }
    switch(v->type) {
        case WT_INTEGER:
            printf("%lld", v->data.i_val);
            break;
        case WT_STRING:
            if(v->data.s_val) printf("%s", v->data.s_val);
            else printf("(null)");
            break;
        case WT_BOOL:
            printf(v->data.b_val ? "true" : "false");
            break;
        case WT_FLOAT:
            printf("%f", v->data.f_val);
            break;
        case WT_SYS_IND:
            printf("<sys_ind:%p>", v->data.ptr_val);
            break;
        default:
            printf("(unknown)");
    }
}

WeltValue* get_var(const char* name) {
    for(int i=0; i<symbol_count; i++) {
        if(strcmp(symbol_table[i].name, name) == 0) {
            return symbol_table[i].val;
        }
    }
    return NULL;
}

void set_var(const char* name, WeltValue* val) {
    if (!name || strlen(name) == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "attempting to set variable with empty name");
        welt_error_syntax(msg, "", current_line_number);
        return;
    }
    
    if (!val) {
        char msg[256];
        snprintf(msg, sizeof(msg), "attempting to set variable '%s' to null value", name);
        welt_error_syntax(msg, name, current_line_number);
        return;
    }
    
    if (strlen(name) >= 64) {
        char msg[256];
        snprintf(msg, sizeof(msg), "variable name '%s' exceeds maximum length of 63 characters", name);
        welt_error_syntax(msg, name, current_line_number);
        return;
    }
    
    for(int i=0; i<symbol_count; i++) {
        if(strcmp(symbol_table[i].name, name) == 0) {
            WeltValue* existing = symbol_table[i].val;
            if(!existing) {
                symbol_table[i].val = val;
                return;
            }
            
            if(existing->is_const) {
                welt_error_const_reassignment(name, current_line_number);
                return;
            }
        
        if(existing->type == WT_STRING && existing->data.s_val) {
            free(existing->data.s_val);
        }
        if(existing->collection_items) {
            for(int i=0; i<existing->collection_size; i++) {
                sa_free(existing->collection_items[i]);
            }
            free(existing->collection_items);
        }
        
        existing->type = val->type;
        existing->is_const = val->is_const;
        existing->is_hex_repr = val->is_hex_repr;
        existing->bit_info = val->bit_info;
        
        if(val->type == WT_STRING && val->data.s_val) {
            existing->data.s_val = strdup(val->data.s_val);
        } else {
            existing->data = val->data;
        }
        
            existing->collection_items = val->collection_items;
            existing->collection_size = val->collection_size;
            existing->collection_cap = val->collection_cap;
            return;
        }
    }
    
    if(symbol_count >= MAX_VARS) {
        welt_error_max_vars(MAX_VARS, current_line_number);
        return;
    }
    
    WeltValue* copy = wv_create();
    copy->type = val->type;
    copy->is_const = val->is_const;
    copy->is_hex_repr = val->is_hex_repr;
    copy->bit_info = val->bit_info;
    
    if(val->type == WT_STRING && val->data.s_val) {
        copy->data.s_val = strdup(val->data.s_val);
    } else {
        copy->data = val->data;
    }
    
    copy->collection_items = val->collection_items;
    copy->collection_size = val->collection_size;
    copy->collection_cap = val->collection_cap;
    
    strcpy(symbol_table[symbol_count].name, name);
    symbol_table[symbol_count].val = copy;
    symbol_count++;
}

// --- Function Execution ---

void execute_function(WeltFunction* func, WeltValue** args, int arg_count);
void parse_line(char* line);

WeltValue* call_function(const char* name, WeltValue** args, int arg_count) {
    WeltFunction* func = get_function(name);
    if (!func) {
        welt_error_undefined_variable(name, current_line_number);
        return NULL;
    }
    
    if (arg_count != func->arg_count) {
        char msg[256];
        snprintf(msg, sizeof(msg), "function '%s' expects %d arguments, got %d", 
                 name, func->arg_count, arg_count);
        welt_error_syntax(msg, name, current_line_number);
        return NULL;
    }
    
    for (int i = 0; i < arg_count; i++) {
        if (!args[i]) {
            char msg[256];
            snprintf(msg, sizeof(msg), "null argument at position %d in call to '%s'", i + 1, name);
            welt_error_syntax(msg, name, current_line_number);
            return NULL;
        }
    }
    
    int saved_symbol_count = symbol_count;
    
    function_return_value = NULL;
    function_has_returned = 0;
    
    for (int i = 0; i < arg_count; i++) {
        set_var(func->args[i], args[i]);
    }
    
    for (int i = 0; i < func->body_line_count; i++) {
        if (function_has_returned) break;
        
        char line_copy[512];
        strncpy(line_copy, func->body_lines[i], sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        parse_line(line_copy);
    }
    
    WeltValue* return_val = function_return_value;
    function_has_returned = 0;
    
    for (int i = saved_symbol_count; i < symbol_count; i++) {
        if (symbol_table[i].val) {
            if (symbol_table[i].val->type == WT_STRING && symbol_table[i].val->data.s_val) {
                free(symbol_table[i].val->data.s_val);
            }
            free(symbol_table[i].val);
            symbol_table[i].val = NULL;
        }
    }
    
    symbol_count = saved_symbol_count;
    
    return return_val;
}

// --- Condition Evaluation ---
int evaluate_condition(char* cond) {
    while(isspace(*cond)) cond++;
    
    WeltValue* left_var = get_var(cond);
    if (left_var && left_var->type == WT_BOOL) {
        return left_var->data.b_val;
    }
    
    char* triple_eq = strstr(cond, "===");
    char* double_eq = strstr(cond, "==");
    char* less_than = strchr(cond, '<');
    char* greater_than = strchr(cond, '>');
    
    if (triple_eq) {
        char left[128], right[128];
        int pos = triple_eq - cond;
        strncpy(left, cond, pos);
        left[pos] = '\0';
        
        char* right_start = triple_eq + 3;
        while(isspace(*right_start)) right_start++;
        strcpy(right, right_start);
        
        while(strlen(left) > 0 && isspace(left[strlen(left)-1])) left[strlen(left)-1] = '\0';
        
        WeltValue* lv = get_var(left);
        WeltValue* rv = get_var(right);
        
        if(lv && rv) {
            return strict_equals(lv, rv);
        }
        
        if(lv && lv->type == WT_INTEGER) {
            long long rval = atoll(right);
            return lv->data.i_val == rval;
        }
        
        return 0;
    } else if (double_eq) {
        char left[128], right[128];
        int pos = double_eq - cond;
        strncpy(left, cond, pos);
        left[pos] = '\0';
        
        char* right_start = double_eq + 2;
        while(isspace(*right_start)) right_start++;
        strcpy(right, right_start);
        
        while(strlen(left) > 0 && isspace(left[strlen(left)-1])) left[strlen(left)-1] = '\0';
        
        WeltValue* lv = get_var(left);
        WeltValue* rv = get_var(right);
        
        if(lv && rv) {
            return loose_equals(lv, rv);
        }
        
        if(lv && lv->type == WT_INTEGER) {
            long long rval = atoll(right);
            return lv->data.i_val == rval;
        }
        
        return 0;
    } else if (less_than && !strstr(cond, "<<")) {
        char left[128], right[128];
        int pos = less_than - cond;
        strncpy(left, cond, pos);
        left[pos] = '\0';
        
        char* right_start = less_than + 1;
        while(isspace(*right_start)) right_start++;
        strcpy(right, right_start);
        
        while(strlen(left) > 0 && isspace(left[strlen(left)-1])) left[strlen(left)-1] = '\0';
        
        WeltValue* lv = get_var(left);
        if(lv && lv->type == WT_INTEGER) {
            long long rval = atoll(right);
            return lv->data.i_val < rval;
        }
    } else if (greater_than && !strstr(cond, ">>")) {
        char left[128], right[128];
        int pos = greater_than - cond;
        strncpy(left, cond, pos);
        left[pos] = '\0';
        
        char* right_start = greater_than + 1;
        while(isspace(*right_start)) right_start++;
        strcpy(right, right_start);
        
        while(strlen(left) > 0 && isspace(left[strlen(left)-1])) left[strlen(left)-1] = '\0';
        
        WeltValue* lv = get_var(left);
        if(lv && lv->type == WT_INTEGER) {
            long long rval = atoll(right);
            return lv->data.i_val > rval;
        }
    }
    
    return 0;
}

// --- Interpreter Logic ---

void parse_line(char* line) {
    // Remove comments
    char* comment = strstr(line, "//");
    if(comment) *comment = '\0';
    
    // Trim whitespace
    while(isspace(*line)) line++;
    if(*line == 0) return;

    // 1. Check for function declaration
    if(strncmp(line, "fn ", 3) == 0) {
        char* func_decl = line + 3;
        char func_name[64];
        char* paren = strchr(func_decl, '(');
        if (paren) {
            int name_len = paren - func_decl;
            strncpy(func_name, func_decl, name_len);
            func_name[name_len] = '\0';
            
            char* trimmed = func_name;
            while(isspace(*trimmed)) trimmed++;
            
            current_function = create_function(trimmed);
            if (!current_function) {
                welt_error_max_vars(100, current_line_number);
                return;
            }
            
            char* args_start = paren + 1;
            char* args_end = strchr(args_start, ')');
            if (args_end) {
                *args_end = '\0';
                
                char* arg = strtok(args_start, ",");
                while (arg && current_function->arg_count < 10) {
                    while(isspace(*arg)) arg++;
                    
                    char* colon = strchr(arg, ':');
                    if (colon) {
                        *colon = '\0';
                        char* type = colon + 1;
                        while(isspace(*type)) type++;
                        
                        char* arg_name = arg;
                        char* angle = strchr(arg_name, '<');
                        if (angle) {
                            *angle = '\0';
                        }
                        
                        strcpy(current_function->arg_types[current_function->arg_count], type);
                        strcpy(current_function->args[current_function->arg_count], arg_name);
                        current_function->arg_count++;
                    }
                    arg = strtok(NULL, ",");
                }
            }
            
            in_function_body = 1;
        }
        return;
    }
    
    // 2. Check for function body end
    if (in_function_body && strcmp(line, "}") == 0) {
        in_function_body = 0;
        current_function = NULL;
        return;
    }
    
    // 3. If in function body, store line
    if (in_function_body && current_function) {
        if (strcmp(line, "{") != 0) {
            add_function_body_line(current_function, line);
        }
        return;
    }
    
    // 4. Check for if statement
    if(strncmp(line, "if ", 3) == 0 || strncmp(line, "if[", 3) == 0) {
        char* cond_start = strchr(line, '[');
        char* cond_end = strchr(line, ']');
        if (cond_start && cond_end) {
            *cond_end = '\0';
            cond_start++;
            if_condition_result = evaluate_condition(cond_start);
            in_if_block = 1;
            skip_if_block = !if_condition_result;
        }
        return;
    }
    
    if(strcmp(line, "} else if[") == 0 || strncmp(line, "} else if [", 11) == 0) {
        char* cond_start = strchr(line, '[');
        char* cond_end = strchr(line, ']');
        if (cond_start && cond_end && !if_condition_result) {
            *cond_end = '\0';
            cond_start++;
            if_condition_result = evaluate_condition(cond_start);
            skip_if_block = !if_condition_result;
        }
        return;
    }
    
    if(strcmp(line, "} else {") == 0 || strcmp(line, "} else") == 0) {
        if (!if_condition_result) {
            skip_if_block = 0;
        } else {
            skip_if_block = 1;
        }
        return;
    }
    
    if(in_if_block && strcmp(line, "}") == 0) {
        in_if_block = 0;
        skip_if_block = 0;
        if_condition_result = 0;
        return;
    }
    
    if(skip_if_block) {
        return;
    }
    
    // 5. Check for for loop
    if(strncmp(line, "for ", 4) == 0 || strncmp(line, "for[", 4) == 0) {
        char* cond_start = strchr(line, '[');
        char* cond_end = strchr(line, ']');
        if (cond_start && cond_end) {
            *cond_end = '\0';
            cond_start++;
            
            char* semi1 = strchr(cond_start, ';');
            char* semi2 = semi1 ? strchr(semi1 + 1, ';') : NULL;
            
            if (semi1 && semi2) {
                *semi1 = '\0';
                *semi2 = '\0';
                strcpy(for_init, cond_start);
                strcpy(for_cond, semi1 + 1);
                strcpy(for_incr, semi2 + 1);
                
                in_for_loop = 1;
                for_body_count = 0;
                
                parse_line(for_init);
            }
        }
        return;
    }
    
    if(in_for_loop) {
        if(strcmp(line, "}") == 0) {
            while(evaluate_condition(for_cond)) {
                for(int i = 0; i < for_body_count; i++) {
                    parse_line(for_body_lines[i]);
                }
                parse_line(for_incr);
            }
            
            in_for_loop = 0;
            for_body_count = 0;
        } else if(strcmp(line, "{") != 0) {
            for_body_lines[for_body_count++] = strdup(line);
        }
        return;
    }
    
    // 6. Check for increment/decrement
    if(strstr(line, "++") || strstr(line, "--")) {
        char var_name[64];
        if(sscanf(line, "%63[^+]", var_name) == 1 || sscanf(line, "%63[^-]", var_name) == 1) {
            WeltValue* v = get_var(var_name);
            if(v && v->type == WT_INTEGER) {
                if(strstr(line, "++")) {
                    v->data.i_val++;
                } else {
                    v->data.i_val--;
                }
            }
        }
        return;
    }
    
    // 7. Check for includes
    if(strncmp(line, "#include", 8) == 0) {
        return;
    }
    
    if(strncmp(line, "#import FSAL", 12) == 0) {
        fsal_imported = 1;
        printf("[FSAL] Module imported\n");
        return;
    }
    
    if(strncmp(line, "FSAL.port(", 10) == 0) {
        if(!fsal_imported) {
            welt_error_syntax("FSAL module not imported. Use #import FSAL first", line, current_line_number);
            return;
        }
        
        char* start = strchr(line, '"');
        if(start) {
            char* end = strrchr(start + 1, '"');
            if(end) {
                *end = '\0';
                const char* filepath = start + 1;
                
                if(fsal_current_file) {
                    fsal_free(fsal_current_file);
                }
                
                fsal_current_file = fsal_load(filepath);
                if(fsal_current_file) {
                    printf("[FSAL] Ported file: %s (%d sections)\n", filepath, fsal_current_file->section_count);
                } else {
                    printf("[FSAL] Error: Failed to load file '%s'\n", filepath);
                }
            }
        }
        return;
    }
    
    if(strncmp(line, "FSAL.addSection(", 16) == 0) {
        if(!fsal_imported) {
            welt_error_syntax("FSAL module not imported. Use #import FSAL first", line, current_line_number);
            return;
        }
        
        printf("[FSAL] Note: addSection is not yet implemented for runtime modification\n");
        return;
    }
    
    if(strncmp(line, "FSAL.addVariable(", 17) == 0) {
        if(!fsal_imported) {
            welt_error_syntax("FSAL module not imported. Use #import FSAL first", line, current_line_number);
            return;
        }
        
        printf("[FSAL] Note: addVariable is not yet implemented for runtime modification\n");
        return;
    }
    
    if(strncmp(line, "FSAL.importWhole(", 17) == 0) {
        if(!fsal_imported) {
            welt_error_syntax("FSAL module not imported. Use #import FSAL first", line, current_line_number);
            return;
        }
        
        if(!fsal_current_file) {
            printf("[FSAL] Error: No file ported. Use FSAL.port() first\n");
            return;
        }
        
        char section_name[128] = {0};
        char* start = strchr(line, '"');
        if(start) {
            char* end = strrchr(start + 1, '"');
            if(end) {
                int len = end - (start + 1);
                strncpy(section_name, start + 1, len);
                section_name[len] = '\0';
            }
        } else {
            char* paren = strchr(line, '(');
            if(paren) {
                char var_name[64];
                char* close = strchr(paren + 1, ')');
                if(close) {
                    int len = close - (paren + 1);
                    strncpy(var_name, paren + 1, len);
                    var_name[len] = '\0';
                    
                    char* trimmed = var_name;
                    while(isspace(*trimmed)) trimmed++;
                    char* end = var_name + strlen(var_name) - 1;
                    while(end > var_name && isspace(*end)) *end-- = '\0';
                    
                    WeltValue* v = get_var(trimmed);
                    if(v && v->type == WT_STRING) {
                        strncpy(section_name, v->data.s_val, sizeof(section_name) - 1);
                    }
                }
            }
        }
        
        if(strlen(section_name) > 0) {
            FSALSection* section = fsal_get_section(fsal_current_file, section_name);
            if(section) {
                for(int i = 0; i < section->entry_count; i++) {
                    FSALEntry* entry = &section->entries[i];
                    WeltValue* v = wv_create();
                    
                    switch(entry->type) {
                        case FSAL_TYPE_INT:
                            v->type = WT_INTEGER;
                            v->data.i_val = entry->value.i_val;
                            break;
                        case FSAL_TYPE_FLOAT:
                            v->type = WT_FLOAT;
                            v->data.f_val = entry->value.f_val;
                            break;
                        case FSAL_TYPE_BOOL:
                            v->type = WT_BOOL;
                            v->data.b_val = entry->value.b_val;
                            break;
                        case FSAL_TYPE_STRING:
                            v->type = WT_STRING;
                            v->data.s_val = strdup(entry->value.s_val);
                            break;
                        default:
                            free(v);
                            continue;
                    }
                    
                    set_var(entry->key, v);
                    printf("[FSAL] Imported %s = ", entry->key);
                    welt_print_value(v);
                    printf("\n");
                }
                printf("[FSAL] Imported %d variables from section '%s'\n", section->entry_count, section_name);
            } else {
                printf("[FSAL] Error: Section '%s' not found\n", section_name);
            }
        }
        return;
    }
    
    if(strncmp(line, "FSAL.importBuilding(", 20) == 0) {
        if(!fsal_imported) {
            welt_error_syntax("FSAL module not imported. Use #import FSAL first", line, current_line_number);
            return;
        }
        
        if(!fsal_current_file) {
            printf("[FSAL] Error: No file ported. Use FSAL.port() first\n");
            return;
        }
        
        char var_name[128] = {0};
        char section_name[128] = {0};
        
        char* paren = strchr(line, '(');
        if(paren) {
            char args[256];
            char* close = strrchr(paren + 1, ')');
            if(close) {
                int len = close - (paren + 1);
                strncpy(args, paren + 1, len);
                args[len] = '\0';
                
                char* comma = strchr(args, ',');
                if(comma) {
                    *comma = '\0';
                    char* first = args;
                    char* second = comma + 1;
                    
                    while(isspace(*first)) first++;
                    char* end1 = first + strlen(first) - 1;
                    while(end1 > first && isspace(*end1)) *end1-- = '\0';
                    
                    while(isspace(*second)) second++;
                    char* end2 = second + strlen(second) - 1;
                    while(end2 > second && isspace(*end2)) *end2-- = '\0';
                    
                    if(first[0] == '"') {
                        char* quote_end = strchr(first + 1, '"');
                        if(quote_end) {
                            int vlen = quote_end - (first + 1);
                            strncpy(var_name, first + 1, vlen);
                            var_name[vlen] = '\0';
                        }
                    } else {
                        WeltValue* v = get_var(first);
                        if(v && v->type == WT_STRING) {
                            strncpy(var_name, v->data.s_val, sizeof(var_name) - 1);
                        }
                    }
                    
                    if(second[0] == '"') {
                        char* quote_end = strchr(second + 1, '"');
                        if(quote_end) {
                            int slen = quote_end - (second + 1);
                            strncpy(section_name, second + 1, slen);
                            section_name[slen] = '\0';
                        }
                    } else {
                        WeltValue* v = get_var(second);
                        if(v && v->type == WT_STRING) {
                            strncpy(section_name, v->data.s_val, sizeof(section_name) - 1);
                        }
                    }
                }
            }
        }
        
        if(strlen(var_name) > 0 && strlen(section_name) > 0) {
            FSALSection* section = fsal_get_section(fsal_current_file, section_name);
            if(section) {
                FSALEntry* entry = fsal_get_entry(section, var_name);
                if(entry) {
                    WeltValue* v = wv_create();
                    
                    switch(entry->type) {
                        case FSAL_TYPE_INT:
                            v->type = WT_INTEGER;
                            v->data.i_val = entry->value.i_val;
                            break;
                        case FSAL_TYPE_FLOAT:
                            v->type = WT_FLOAT;
                            v->data.f_val = entry->value.f_val;
                            break;
                        case FSAL_TYPE_BOOL:
                            v->type = WT_BOOL;
                            v->data.b_val = entry->value.b_val;
                            break;
                        case FSAL_TYPE_STRING:
                            v->type = WT_STRING;
                            v->data.s_val = strdup(entry->value.s_val);
                            break;
                        default:
                            free(v);
                            return;
                    }
                    
                    set_var(entry->key, v);
                    printf("[FSAL] Imported %s = ", entry->key);
                    welt_print_value(v);
                    printf(" from section '%s'\n", section_name);
                } else {
                    printf("[FSAL] Error: Variable '%s' not found in section '%s'\n", var_name, section_name);
                }
            } else {
                printf("[FSAL] Error: Section '%s' not found\n", section_name);
            }
        }
        return;
    }
    
    // 5. Check for function calls
    char* paren = strchr(line, '(');
    if (paren && !strstr(line, "print(") && !strstr(line, "=")) {
        char func_name[64];
        int name_len = paren - line;
        strncpy(func_name, line, name_len);
        func_name[name_len] = '\0';
        
        char* trimmed = func_name;
        while(isspace(*trimmed)) trimmed++;
        char* end = func_name + strlen(func_name) - 1;
        while(end > func_name && isspace(*end)) *end-- = '\0';
        
        WeltFunction* target_func = get_function(trimmed);
        if (target_func) {
            WeltValue* args[10];
            WeltValue* temp_literals[10];
            int arg_count = 0;
            int temp_count = 0;
            
            char* args_start = paren + 1;
            char* args_end = strrchr(args_start, ')');
            if (args_end) *args_end = '\0';
            
            char args_copy[256];
            strncpy(args_copy, args_start, sizeof(args_copy) - 1);
            args_copy[sizeof(args_copy) - 1] = '\0';
            
            if (strlen(args_copy) > 0) {
                char* arg = strtok(args_copy, ",");
                while (arg && arg_count < 10) {
                    while(isspace(*arg)) arg++;
                    char* arg_end = arg + strlen(arg) - 1;
                    while(arg_end > arg && isspace(*arg_end)) *arg_end-- = '\0';
                    
                    if (strlen(arg) == 0) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "empty argument in function call '%s'", trimmed);
                        welt_error_syntax(msg, line, current_line_number);
                        for(int k=0; k<temp_count; k++) free(temp_literals[k]);
                        return;
                    }
                    
                    WeltValue* v = NULL;
                    
                    if (arg[0] == '"') {
                        char* closing = strrchr(arg + 1, '"');
                        if (!closing) {
                            char msg[256];
                            snprintf(msg, sizeof(msg), "unclosed string literal in argument %d", arg_count + 1);
                            welt_error_syntax(msg, arg, current_line_number);
                            for(int k=0; k<temp_count; k++) free(temp_literals[k]);
                            return;
                        }
                        *closing = '\0';
                        v = wv_create();
                        v->type = WT_STRING;
                        v->data.s_val = strdup(arg + 1);
                        temp_literals[temp_count++] = v;
                    } else if (strcmp(arg, "true") == 0 || strcmp(arg, "false") == 0) {
                        v = wv_create();
                        v->type = WT_BOOL;
                        v->data.b_val = (strcmp(arg, "true") == 0) ? 1 : 0;
                        temp_literals[temp_count++] = v;
                    } else if ((arg[0] >= '0' && arg[0] <= '9') || arg[0] == '-') {
                        int is_float = 0;
                        for(int k=0; arg[k]; k++) {
                            if(arg[k] == '.') { is_float = 1; break; }
                        }
                        v = wv_create();
                        if(is_float) {
                            v->type = WT_FLOAT;
                            v->data.f_val = atof(arg);
                        } else {
                            v->type = WT_INTEGER;
                            v->data.i_val = atoll(arg);
                        }
                        temp_literals[temp_count++] = v;
                    } else {
                        v = get_var(arg);
                        if (!v) {
                            char msg[256];
                            snprintf(msg, sizeof(msg), "undefined variable '%s' in function call '%s'", arg, trimmed);
                            welt_error_undefined_variable(arg, current_line_number);
                            for(int k=0; k<temp_count; k++) free(temp_literals[k]);
                            return;
                        }
                    }
                    
                    if (v) {
                        args[arg_count++] = v;
                    }
                    arg = strtok(NULL, ",");
                }
            }
            
            if (arg_count != target_func->arg_count) {
                char msg[256];
                snprintf(msg, sizeof(msg), "function '%s' expects %d arguments, got %d", 
                         trimmed, target_func->arg_count, arg_count);
                welt_error_syntax(msg, line, current_line_number);
                for(int k=0; k<temp_count; k++) {
                    if(temp_literals[k]->type == WT_STRING && temp_literals[k]->data.s_val) {
                        free(temp_literals[k]->data.s_val);
                    }
                    free(temp_literals[k]);
                }
                return;
            }
            
            call_function(trimmed, args, arg_count);
            
            for(int k=0; k<temp_count; k++) {
                if(temp_literals[k]->type == WT_STRING && temp_literals[k]->data.s_val) {
                    free(temp_literals[k]->data.s_val);
                }
                free(temp_literals[k]);
            }
            return;
        }
    }

    // 6. Variable reassignment (no type keyword)
    char* equals = strchr(line, '=');
    char* paren_check = strchr(line, '(');
    int has_func_call = (paren_check && paren_check < equals);
    
    if (equals && !strstr(line, "==") && !strstr(line, "===") && strchr(line, ';') && 
        strncmp(line, "print(", 6) != 0 && !has_func_call) {
        char first_word[64];
        sscanf(line, "%63s", first_word);
        
        int is_type_keyword = (strcmp(first_word, "integer") == 0 || 
                               strcmp(first_word, "string") == 0 ||
                               strcmp(first_word, "bool") == 0 ||
                               strcmp(first_word, "float") == 0 ||
                               strcmp(first_word, "ss") == 0 ||
                               strcmp(first_word, "sys_ind") == 0 ||
                               strcmp(first_word, "const") == 0);
        
        if (!is_type_keyword) {
            char var_name[64];
            int name_len = equals - line;
            if (name_len > 0 && name_len < 64) {
                strncpy(var_name, line, name_len);
                var_name[name_len] = '\0';
                
                char* trimmed = var_name;
                while(isspace(*trimmed)) trimmed++;
                char* end = var_name + strlen(var_name) - 1;
                while(end > var_name && isspace(*end)) *end-- = '\0';
                
                WeltValue* existing = get_var(trimmed);
                if (existing) {
                    if (existing->is_const) {
                        welt_error_const_reassignment(trimmed, current_line_number);
                        return;
                    }
                    
                    char* val = equals + 1;
                    while(isspace(*val)) val++;
                    char* semi = strchr(val, ';');
                    if (semi) *semi = '\0';
                    
                    if (existing->type == WT_INTEGER) {
                        if(strstr(val, "<hex>")) {
                            existing->data.i_val = parse_hex_literal(val);
                            existing->is_hex_repr = 1;
                        } else {
                            existing->data.i_val = atoll(val);
                        }
                    } else if (existing->type == WT_STRING) {
                        if (existing->data.s_val) free(existing->data.s_val);
                        char* start = strchr(val, '"');
                        if(start) {
                            char* end = strrchr(start+1, '"');
                            if(end) *end = 0;
                            existing->data.s_val = strdup(start+1);
                        }
                    } else if (existing->type == WT_FLOAT) {
                        existing->data.f_val = atof(val);
                    } else if (existing->type == WT_BOOL) {
                        if(strcmp(val, "true") == 0 || strcmp(val, "1") == 0) {
                            existing->data.b_val = 1;
                        } else {
                            existing->data.b_val = 0;
                        }
                    }
                    return;
                } else {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "cannot reassign undeclared variable '%s'", trimmed);
                    welt_error_undefined_variable(trimmed, current_line_number);
                    return;
                }
            }
        }
    }
    
    // 7. Check for print("...")
    if(strncmp(line, "print(", 6) == 0) {
        char* content = line + 6;
        char* end = strrchr(content, ')');
        if(end) *end = 0;
        
        char* comma = NULL;
        if(content[0] == '"') {
            char* closing_quote = strchr(content + 1, '"');
            if(closing_quote) {
                comma = strchr(closing_quote, ',');
            }
        } else {
            comma = strchr(content, ',');
        }
        
        WeltValue* args[10];
        int arg_count = 0;
        
        if(comma) {
            *comma = 0;
            char* args_str = comma + 1;
            
            if(content[0] == '"') content++;
            char* closing_quote = strrchr(content, '"');
            if(closing_quote) *closing_quote = 0;
            
            char args_copy[256];
            strncpy(args_copy, args_str, sizeof(args_copy) - 1);
            args_copy[sizeof(args_copy) - 1] = '\0';
            
            char* arg_ptrs[10];
            int num_args = split_args_respecting_parens(args_copy, arg_ptrs, 10);
            
            for (int arg_idx = 0; arg_idx < num_args && arg_count < 10; arg_idx++) {
                char* arg = arg_ptrs[arg_idx];
                while(isspace(*arg)) arg++;
                char* arg_end = arg + strlen(arg) - 1;
                while(arg_end > arg && isspace(*arg_end)) *arg_end-- = '\0';
                
                char* paren = strchr(arg, '(');
                if (paren) {
                    char func_name[64];
                    int name_len = paren - arg;
                    strncpy(func_name, arg, name_len);
                    func_name[name_len] = '\0';
                    
                    char* trimmed = func_name;
                    while(isspace(*trimmed)) trimmed++;
                    char* end = func_name + strlen(func_name) - 1;
                    while(end > func_name && isspace(*end)) *end-- = '\0';
                    
                    WeltFunction* target_func = get_function(trimmed);
                    if (target_func) {
                        WeltValue* func_args[10];
                        WeltValue* temp_literals[10];
                        int func_arg_count = 0;
                        int temp_count = 0;
                        
                        char* args_start = paren + 1;
                        char* args_end = strrchr(args_start, ')');
                        if (args_end) *args_end = '\0';
                        
                        char func_args_copy[256];
                        strncpy(func_args_copy, args_start, sizeof(func_args_copy) - 1);
                        func_args_copy[sizeof(func_args_copy) - 1] = '\0';
                        
                        if (strlen(func_args_copy) > 0) {
                            char* func_arg = strtok(func_args_copy, ",");
                            while (func_arg && func_arg_count < 10) {
                                while(isspace(*func_arg)) func_arg++;
                                char* func_arg_end = func_arg + strlen(func_arg) - 1;
                                while(func_arg_end > func_arg && isspace(*func_arg_end)) *func_arg_end-- = '\0';
                                
                                WeltValue* fv = NULL;
                                
                                if (func_arg[0] == '"') {
                                    char* closing = strrchr(func_arg + 1, '"');
                                    if (closing) {
                                        *closing = '\0';
                                        fv = wv_create();
                                        fv->type = WT_STRING;
                                        fv->data.s_val = strdup(func_arg + 1);
                                        temp_literals[temp_count++] = fv;
                                    }
                                } else if (strcmp(func_arg, "true") == 0 || strcmp(func_arg, "false") == 0) {
                                    fv = wv_create();
                                    fv->type = WT_BOOL;
                                    fv->data.b_val = (strcmp(func_arg, "true") == 0);
                                    temp_literals[temp_count++] = fv;
                                } else if (strchr(func_arg, '.')) {
                                    fv = wv_create();
                                    fv->type = WT_FLOAT;
                                    fv->data.f_val = atof(func_arg);
                                    temp_literals[temp_count++] = fv;
                                } else if (isdigit(func_arg[0]) || (func_arg[0] == '-' && isdigit(func_arg[1]))) {
                                    fv = wv_create();
                                    fv->type = WT_INTEGER;
                                    fv->data.i_val = atoll(func_arg);
                                    temp_literals[temp_count++] = fv;
                                } else {
                                    fv = get_var(func_arg);
                                }
                                
                                if (fv) {
                                    func_args[func_arg_count++] = fv;
                                }
                                
                                func_arg = strtok(NULL, ",");
                            }
                        }
                        
                        WeltValue* result = call_function(trimmed, func_args, func_arg_count);
                        
                        for(int k=0; k<temp_count; k++) free(temp_literals[k]);
                        
                        if (result) {
                            args[arg_count++] = result;
                        } else {
                            WeltValue* nil = wv_create();
                            nil->type = WT_INTEGER;
                            nil->data.i_val = 0;
                            args[arg_count++] = nil;
                        }
                    } else {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "undefined function '%s' in print statement", trimmed);
                        welt_error_undefined_variable(trimmed, current_line_number);
                    }
                } else {
                    WeltValue* v = get_var(arg);
                    if (v) {
                        args[arg_count++] = v;
                    } else {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "undefined variable '%s' in print statement", arg);
                        welt_error_undefined_variable(arg, current_line_number);
                    }
                }
            }
            
            welt_print(content, args, arg_count);
        } else {
            if(content[0] == '"') content++;
            char* closing_quote = strrchr(content, '"');
            if(closing_quote) *closing_quote = 0;
            welt_print(content, args, 0);
        }
        return;
    }
    
    // 7. Handle FSAL functions (BEFORE variable declarations!)
    if(strstr(line, "fsal_load(") || strstr(line, "fsal_get_")) {
        goto handle_fsal;
    }

    // 8. Variable Declaration (with optional const)
    char type_buf[32], name_buf[64], val_buf[256];
    int is_const = 0;
    
    if(strncmp(line, "const ", 6) == 0) {
        is_const = 1;
        line += 6;
        while(isspace(*line)) line++;
    }
    
    if (sscanf(line, "%31s %63s = %255[^;];", type_buf, name_buf, val_buf) == 3) {
        WeltValue* v = wv_create();
        v->is_const = is_const;
        
        if(strcmp(type_buf, "integer") == 0) {
            v->type = WT_INTEGER;
            
            if(strstr(val_buf, "<hex>")) {
                v->data.i_val = parse_hex_literal(val_buf);
                v->is_hex_repr = 1;
            } else if(strchr(val_buf, '(')) {
                char* paren = strchr(val_buf, '(');
                char func_name[64];
                int name_len = paren - val_buf;
                strncpy(func_name, val_buf, name_len);
                func_name[name_len] = '\0';
                
                char* trimmed = func_name;
                while(isspace(*trimmed)) trimmed++;
                
                char* args_start = paren + 1;
                char* args_end = strrchr(args_start, ')');
                if(args_end) *args_end = '\0';
                
                WeltValue* temp_args[10];
                WeltValue* temp_literals[10];
                int arg_count = 0;
                int temp_count = 0;
                
                if(strlen(args_start) > 0) {
                    char* arg_ptrs[10];
                    char args_copy[256];
                    strncpy(args_copy, args_start, sizeof(args_copy) - 1);
                    args_copy[sizeof(args_copy) - 1] = '\0';
                    int num_args = split_args_respecting_parens(args_copy, arg_ptrs, 10);
                    
                    for(int i = 0; i < num_args && arg_count < 10; i++) {
                        char* arg = arg_ptrs[i];
                        while(isspace(*arg)) arg++;
                        char* arg_end = arg + strlen(arg) - 1;
                        while(arg_end > arg && isspace(*arg_end)) *arg_end-- = '\0';
                        
                        WeltValue* av = NULL;
                        if(isdigit(arg[0]) || (arg[0] == '-' && isdigit(arg[1]))) {
                            av = wv_create();
                            av->type = WT_INTEGER;
                            av->data.i_val = atoll(arg);
                            temp_literals[temp_count++] = av;
                        } else {
                            av = get_var(arg);
                        }
                        if(av) temp_args[arg_count++] = av;
                    }
                }
                
                WeltValue* result = call_function(trimmed, temp_args, arg_count);
                for(int k=0; k<temp_count; k++) free(temp_literals[k]);
                
                if(result && result->type == WT_INTEGER) {
                    v->data.i_val = result->data.i_val;
                } else {
                    v->data.i_val = 0;
                }
            } else {
                char* plus = strchr(val_buf, '+');
                char* minus = strrchr(val_buf, '-');
                char* mult = strchr(val_buf, '*');
                
                if(plus) {
                    *plus = '\0';
                    char* left = val_buf;
                    char* right = plus + 1;
                    while(isspace(*left)) left++;
                    while(isspace(*right)) right++;
                    
                    char left_trim[128], right_trim[128];
                    strncpy(left_trim, left, sizeof(left_trim) - 1);
                    left_trim[sizeof(left_trim) - 1] = '\0';
                    strncpy(right_trim, right, sizeof(right_trim) - 1);
                    right_trim[sizeof(right_trim) - 1] = '\0';
                    
                    char* end_left = left_trim + strlen(left_trim) - 1;
                    while(end_left > left_trim && isspace(*end_left)) *end_left-- = '\0';
                    char* end_right = right_trim + strlen(right_trim) - 1;
                    while(end_right > right_trim && isspace(*end_right)) *end_right-- = '\0';
                    
                    WeltValue* lv = get_var(left_trim);
                    WeltValue* rv = get_var(right_trim);
                    
                    if(lv && rv && lv->type == WT_INTEGER && rv->type == WT_INTEGER) {
                        v->data.i_val = lv->data.i_val + rv->data.i_val;
                    } else {
                        v->data.i_val = atoll(left_trim) + atoll(right_trim);
                    }
                } else if(minus && minus != val_buf && !strchr(val_buf, '(')) {
                    *minus = '\0';
                    char* left = val_buf;
                    char* right = minus + 1;
                    while(isspace(*left)) left++;
                    while(isspace(*right)) right++;
                    
                    WeltValue* lv = get_var(left);
                    WeltValue* rv = get_var(right);
                    
                    if(lv && rv && lv->type == WT_INTEGER && rv->type == WT_INTEGER) {
                        v->data.i_val = lv->data.i_val - rv->data.i_val;
                    } else {
                        v->data.i_val = atoll(left) - atoll(right);
                    }
                } else if(mult && !strchr(val_buf, '(')) {
                    *mult = '\0';
                    char* left = val_buf;
                    char* right = mult + 1;
                    while(isspace(*left)) left++;
                    while(isspace(*right)) right++;
                    
                    WeltValue* lv = get_var(left);
                    WeltValue* rv = get_var(right);
                    
                    if(lv && rv && lv->type == WT_INTEGER && rv->type == WT_INTEGER) {
                        v->data.i_val = lv->data.i_val * rv->data.i_val;
                    } else {
                        v->data.i_val = atoll(left) * atoll(right);
                    }
                } else {
                    v->data.i_val = atoll(val_buf);
                }
            }
        } 
        else if(strcmp(type_buf, "string") == 0) {
            v->type = WT_STRING;
            char* start = strchr(val_buf, '"');
            if(start) {
                char* end = strrchr(start+1, '"');
                if(end) *end = 0;
                v->data.s_val = strdup(start+1);
            }
        }
        else if(strcmp(type_buf, "bool") == 0) {
            v->type = WT_BOOL;
            if(strcmp(val_buf, "true") == 0 || strcmp(val_buf, "1") == 0) {
                v->data.b_val = 1;
            } else {
                v->data.b_val = 0;
            }
        }
        else if(strcmp(type_buf, "float") == 0) {
            v->type = WT_FLOAT;
            v->data.f_val = atof(val_buf);
        }
        else if(strcmp(type_buf, "sys_ind") == 0) {
            v->type = WT_SYS_IND;
            v->data.ptr_val = NULL;
        }
        else if(strcmp(type_buf, "ss") == 0) {
            if(strchr(val_buf, '"')) {
                v->type = WT_STRING;
                char* start = strchr(val_buf, '"');
                if(start) {
                    char* end = strrchr(start+1, '"');
                    if(end) *end = 0;
                    v->data.s_val = strdup(start+1);
                } else {
                    v->data.s_val = strdup(val_buf);
                }
            } else {
                v->type = WT_INTEGER;
                v->data.i_val = atoll(val_buf);
            }
        }
        else {
            welt_error_unknown_keyword(type_buf, current_line_number);
            sa_free(v);
            return;
        }
        
        if(val_buf[0] == '{' || val_buf[0] == '[') {
            char bracket_type = val_buf[0];
            v->type = (bracket_type == '{') ? WT_ARRAY : WT_TABLE;
            v->collection_items = NULL;
            v->collection_size = 0;
            v->collection_cap = 0;
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
    
    // 8. Handle return statement
    if(strncmp(line, "return", 6) == 0) {
        char* ret_val = line + 6;
        while(isspace(*ret_val)) ret_val++;
        
        char* semi = strchr(ret_val, ';');
        if(semi) *semi = '\0';
        
        char* end = ret_val + strlen(ret_val) - 1;
        while(end > ret_val && isspace(*end)) *end-- = '\0';
        
        if (strlen(ret_val) == 0) {
            function_return_value = NULL;
            function_has_returned = 1;
            return;
        }
        
        WeltValue* v = wv_create();
        
        if (ret_val[0] == '"') {
            char* closing = strrchr(ret_val + 1, '"');
            if (closing) {
                *closing = '\0';
                v->type = WT_STRING;
                v->data.s_val = strdup(ret_val + 1);
            }
        } else if (strcmp(ret_val, "true") == 0 || strcmp(ret_val, "false") == 0) {
            v->type = WT_BOOL;
            v->data.b_val = (strcmp(ret_val, "true") == 0);
        } else if (strchr(ret_val, '.')) {
            v->type = WT_FLOAT;
            v->data.f_val = atof(ret_val);
        } else if (isdigit(ret_val[0]) || (ret_val[0] == '-' && isdigit(ret_val[1]))) {
            v->type = WT_INTEGER;
            v->data.i_val = atoll(ret_val);
        } else {
            WeltValue* var = get_var(ret_val);
            if (var) {
                free(v);
                v = wv_create();
                v->type = var->type;
                if (var->type == WT_STRING && var->data.s_val) {
                    v->data.s_val = strdup(var->data.s_val);
                } else {
                    v->data = var->data;
                }
            } else {
                v->type = WT_INTEGER;
                v->data.i_val = 0;
            }
        }
        
        function_return_value = v;
        function_has_returned = 1;
        return;
    }
    
    // 9. Handle structure and class
    if(strncmp(line, "structure ", 10) == 0) {
        char struct_name[64];
        sscanf(line + 10, "%63s", struct_name);
        return;
    }
    
    if(strncmp(line, "class ", 6) == 0) {
        char class_name[64];
        sscanf(line + 6, "%63s", class_name);
        return;
    }
    
    // 10. Handle global_extentions
    if(strncmp(line, "global_extentions ", 18) == 0) {
        char ext_name[64];
        sscanf(line + 18, "%63s", ext_name);
        return;
    }
    
    // 11. Handle method calls (.has(), .bytes(), .length_of_variable(), etc)
    char* dot = strchr(line, '.');
    if(dot && strchr(dot, '(')) {
        char var_name[64];
        int name_len = dot - line;
        if(name_len > 0 && name_len < 64) {
            strncpy(var_name, line, name_len);
            var_name[name_len] = '\0';
            
            char* method_name = dot + 1;
            char* paren = strchr(method_name, '(');
            if(paren) {
                int method_len = paren - method_name;
                char method[64];
                strncpy(method, method_name, method_len);
                method[method_len] = '\0';
                
                WeltValue* v = get_var(var_name);
                if(v) {
                    if(strcmp(method, "has") == 0) {
                        return;
                    } else if(strcmp(method, "length_of_variable") == 0) {
                        int len = wv_length(v);
                        printf("%d\n", len);
                        return;
                    } else if(strcmp(method, "bytes") == 0) {
                        return;
                    } else {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "unknown method '%s' for variable '%s'", method, var_name);
                        welt_error_syntax(msg, line, current_line_number);
                        return;
                    }
                } else {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "cannot call method '%s' on undefined variable '%s'", method, var_name);
                    welt_error_undefined_variable(var_name, current_line_number);
                    return;
                }
            }
        }
        return;
    }
    
    // 12. Handle ternary operator
    if(strstr(line, "?") && strstr(line, ":")) {
        return;
    }
    
    // 13. Handle multiple variable declarations [var1, var2, var3]
    if(strchr(line, '[') && strchr(line, ',')) {
        char type[32];
        if(sscanf(line, "%31s", type) == 1) {
            int is_type = (strcmp(type, "integer") == 0 || 
                          strcmp(type, "string") == 0 ||
                          strcmp(type, "bool") == 0 ||
                          strcmp(type, "float") == 0);
            if(is_type) {
                return;
            }
        }
    }
    
    // 14. Handle fwww_at
    if(strncmp(line, "fwww_at(", 8) == 0) {
        // Parse args
        // fwww_at("url", varname)
        // ... simplified extraction ...
    }
    
handle_fsal:
    // 15. Handle FSAL functions
    if(strncmp(line, "fsal_load(", 10) == 0) {
        char* start = strchr(line, '"');
        if (start) {
            char* end = strrchr(start + 1, '"');
            if (end) {
                *end = '\0';
                WeltValue* v = welt_fsal_load(start + 1);
                if (v) {
                    char var_name[64];
                    sscanf(line, "sys_ind %63s", var_name);
                    char* eq = strchr(var_name, '=');
                    if (eq) *eq = '\0';
                    set_var(var_name, v);
                }
            }
        }
        return;
    }
    
    if(strncmp(line, "fsal_get_string(", 16) == 0) {
        char var_name[64];
        char* start = strchr(line, '(');
        if (!start) return;
        
        sscanf(line, "string %63s =", var_name);
        
        char* end = strrchr(start, ')');
        if (!end) return;
        
        char args_buf[512];
        int args_len = end - (start + 1);
        strncpy(args_buf, start + 1, args_len);
        args_buf[args_len] = '\0';
        
        char file_var[64] = {0};
        char section_str[128] = {0};
        char key_str[128] = {0};
        
        int matches = sscanf(args_buf, "%63[^,], \"%127[^\"]\", \"%127[^\"]\"", file_var, section_str, key_str);
        printf("DEBUG fsal_get_string: args_buf='%s', matches=%d\n", args_buf, matches);
        printf("DEBUG file_var='%s', section='%s', key='%s'\n", file_var, section_str, key_str);
        
        WeltValue* file_v = get_var(file_var);
        if (file_v && file_v->type == WT_SYS_IND) {
            printf("DEBUG: Found file pointer\n");
            WeltValue* result = welt_fsal_get_string((FSALFile*)file_v->data.ptr_val, section_str, key_str);
            if (result) {
                printf("DEBUG: Got result, setting var %s\n", var_name);
                set_var(var_name, result);
            } else {
                printf("DEBUG: No result from fsal_get_string\n");
            }
        } else {
            printf("DEBUG: file_v is NULL or not SYS_IND\n");
        }
        return;
    }
    
    if(strncmp(line, "fsal_get_int(", 13) == 0) {
        char var_name[64];
        char* start = strchr(line, '(');
        if (!start) return;
        
        sscanf(line, "integer %63s =", var_name);
        
        char* end = strrchr(start, ')');
        if (!end) return;
        
        char args_buf[512];
        int args_len = end - (start + 1);
        strncpy(args_buf, start + 1, args_len);
        args_buf[args_len] = '\0';
        
        char file_var[64] = {0};
        char section_str[128] = {0};
        char key_str[128] = {0};
        
        sscanf(args_buf, "%63[^,], \"%127[^\"]\", \"%127[^\"]\"", file_var, section_str, key_str);
        
        WeltValue* file_v = get_var(file_var);
        if (file_v && file_v->type == WT_SYS_IND) {
            WeltValue* result = welt_fsal_get_int((FSALFile*)file_v->data.ptr_val, section_str, key_str);
            if (result) {
                set_var(var_name, result);
            }
        }
        return;
    }
    
    if(strncmp(line, "fsal_get_bool(", 14) == 0) {
        char var_name[64];
        char* start = strchr(line, '(');
        if (!start) return;
        
        sscanf(line, "bool %63s =", var_name);
        
        char* end = strrchr(start, ')');
        if (!end) return;
        
        char args_buf[512];
        int args_len = end - (start + 1);
        strncpy(args_buf, start + 1, args_len);
        args_buf[args_len] = '\0';
        
        char file_var[64] = {0};
        char section_str[128] = {0};
        char key_str[128] = {0};
        
        sscanf(args_buf, "%63[^,], \"%127[^\"]\", \"%127[^\"]\"", file_var, section_str, key_str);
        
        WeltValue* file_v = get_var(file_var);
        if (file_v && file_v->type == WT_SYS_IND) {
            WeltValue* result = welt_fsal_get_bool((FSALFile*)file_v->data.ptr_val, section_str, key_str);
            if (result) {
                set_var(var_name, result);
            }
        }
        return;
    }
    
    if(strncmp(line, "fsal_get_float(", 15) == 0) {
        char var_name[64];
        char* start = strchr(line, '(');
        if (!start) return;
        
        sscanf(line, "float %63s =", var_name);
        
        char* end = strrchr(start, ')');
        if (!end) return;
        
        char args_buf[512];
        int args_len = end - (start + 1);
        strncpy(args_buf, start + 1, args_len);
        args_buf[args_len] = '\0';
        
        char file_var[64] = {0};
        char section_str[128] = {0};
        char key_str[128] = {0};
        
        sscanf(args_buf, "%63[^,], \"%127[^\"]\", \"%127[^\"]\"", file_var, section_str, key_str);
        
        WeltValue* file_v = get_var(file_var);
        if (file_v && file_v->type == WT_SYS_IND) {
            WeltValue* result = welt_fsal_get_float((FSALFile*)file_v->data.ptr_val, section_str, key_str);
            if (result) {
                set_var(var_name, result);
            }
        }
        return;
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
    current_line_number = 0;

    while (fgets(line, sizeof(line), fp)) {
        current_line_number++;
        line[strcspn(line, "\n")] = 0;
        
        if(in_block_comment) {
            if(strstr(line, "*/")) in_block_comment = 0;
            continue;
        }
        
        char* block_start = strstr(line, "/*");
        if(block_start) {
            char* block_end = strstr(block_start, "*/");
            if(block_end) {
                *block_start = '\0';
                parse_line(line);
            } else {
                in_block_comment = 1;
            }
            continue;
        }

        parse_line(line);
    }

    fclose(fp);
    
    WeltFunction* main_func = get_function("main");
    if (main_func) {
        WeltValue* args[10];
        call_function("main", args, 0);
    } else {
        printf("\033[1;31merror\033[0m: no 'main' function found\n");
        printf("   \033[1;34m= \033[1mhelp\033[0m: add a main function as the program entry point:\n");
        printf("           \033[1;32mfn main() {\033[0m\n");
        printf("           \033[1;32m    // your code here\033[0m\n");
        printf("           \033[1;32m}\033[0m\n");
        welt_fsal_cleanup();
        return 1;
    }
    
    welt_fsal_cleanup();
    
    if(fsal_current_file) {
        fsal_free(fsal_current_file);
        fsal_current_file = NULL;
    }
    
    return 0;
}