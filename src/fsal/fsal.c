#include "fsal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint16_t read_u16(FILE* f) {
    uint8_t buf[2];
    if (fread(buf, 1, 2, f) != 2) {
        fprintf(stderr, "FSAL Error: Failed to read uint16\n");
        return 0;
    }
    return (buf[0] << 8) | buf[1];
}

static uint32_t read_u32(FILE* f) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, f) != 4) {
        fprintf(stderr, "FSAL Error: Failed to read uint32\n");
        return 0;
    }
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static uint8_t read_u8(FILE* f) {
    uint8_t val;
    if (fread(&val, 1, 1, f) != 1) {
        fprintf(stderr, "FSAL Error: Failed to read uint8\n");
        return 0;
    }
    return val;
}

static int32_t read_i32(FILE* f) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, f) != 4) {
        fprintf(stderr, "FSAL Error: Failed to read int32\n");
        return 0;
    }
    int32_t val = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return val;
}

static float read_float(FILE* f) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, f) != 4) {
        fprintf(stderr, "FSAL Error: Failed to read float\n");
        return 0.0f;
    }
    uint32_t temp = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    float val;
    memcpy(&val, &temp, sizeof(float));
    return val;
}

static char* read_string(FILE* f, uint32_t len) {
    if (len == 0 || len > FSAL_MAX_STRING_LEN * 10) {
        fprintf(stderr, "FSAL Error: Invalid string length %u\n", len);
        return NULL;
    }
    
    char* str = malloc(len + 1);
    if (!str) {
        fprintf(stderr, "FSAL Error: Memory allocation failed for string\n");
        return NULL;
    }
    
    if (fread(str, 1, len, f) != len) {
        fprintf(stderr, "FSAL Error: Failed to read string data\n");
        free(str);
        return NULL;
    }
    
    str[len] = '\0';
    return str;
}

static FSALTable* read_table(FILE* f, uint32_t size) {
    long pos = ftell(f);
    uint32_t count = read_u32(f);
    
    if (count > FSAL_MAX_TABLE_ENTRIES) {
        fprintf(stderr, "FSAL Error: Table has too many entries: %u\n", count);
        return NULL;
    }
    
    FSALTable* table = malloc(sizeof(FSALTable));
    if (!table) {
        fprintf(stderr, "FSAL Error: Failed to allocate table\n");
        return NULL;
    }
    
    table->entries = malloc(sizeof(FSALEntry) * count);
    if (!table->entries) {
        fprintf(stderr, "FSAL Error: Failed to allocate table entries\n");
        free(table);
        return NULL;
    }
    
    table->entry_count = count;
    
    for (uint32_t i = 0; i < count; i++) {
        uint16_t key_len = read_u16(f);
        char* key = read_string(f, key_len);
        if (!key) {
            for (uint32_t j = 0; j < i; j++) {
                free(table->entries[j].key);
                if (table->entries[j].type == FSAL_TYPE_STRING) {
                    free(table->entries[j].value.s_val);
                }
            }
            free(table->entries);
            free(table);
            return NULL;
        }
        
        uint8_t type_id = read_u8(f);
        uint32_t val_size = read_u32(f);
        
        table->entries[i].key = key;
        table->entries[i].type = (FSALType)type_id;
        
        switch (type_id) {
            case FSAL_TYPE_INT:
                table->entries[i].value.i_val = read_i32(f);
                break;
            case FSAL_TYPE_FLOAT:
                table->entries[i].value.f_val = read_float(f);
                break;
            case FSAL_TYPE_BOOL:
                table->entries[i].value.b_val = read_u8(f);
                break;
            case FSAL_TYPE_STRING:
                table->entries[i].value.s_val = read_string(f, val_size);
                break;
            default:
                fprintf(stderr, "FSAL Error: Unknown type in table: %u\n", type_id);
                break;
        }
    }
    
    return table;
}

FSALFile* fsal_load(const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "FSAL Error: Cannot open file '%s'\n", filepath);
        return NULL;
    }
    
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, FSAL_MAGIC, 4) != 0) {
        fprintf(stderr, "FSAL Error: Invalid ATFF magic bytes\n");
        fclose(f);
        return NULL;
    }
    
    FSALFile* file = malloc(sizeof(FSALFile));
    if (!file) {
        fprintf(stderr, "FSAL Error: Memory allocation failed\n");
        fclose(f);
        return NULL;
    }
    
    memset(file, 0, sizeof(FSALFile));
    
    uint32_t link_count = read_u32(f);
    if (link_count > 0) {
        file->links = malloc(sizeof(char*) * link_count);
        file->link_count = link_count;
        
        for (uint32_t i = 0; i < link_count; i++) {
            uint16_t len = read_u16(f);
            file->links[i] = read_string(f, len);
            if (!file->links[i]) {
                fprintf(stderr, "FSAL Warning: Failed to read link %u\n", i);
            } else {
                printf("[FSAL] Found link: %s (execution disabled in C version)\n", file->links[i]);
            }
        }
    }
    
    uint32_t section_count = read_u32(f);
    if (section_count > FSAL_MAX_SECTIONS) {
        fprintf(stderr, "FSAL Error: Too many sections: %u\n", section_count);
        fsal_free(file);
        fclose(f);
        return NULL;
    }
    
    file->sections = malloc(sizeof(FSALSection) * section_count);
    if (!file->sections) {
        fprintf(stderr, "FSAL Error: Failed to allocate sections\n");
        fsal_free(file);
        fclose(f);
        return NULL;
    }
    
    file->section_count = section_count;
    
    for (uint32_t si = 0; si < section_count; si++) {
        uint16_t name_len = read_u16(f);
        file->sections[si].name = read_string(f, name_len);
        
        if (!file->sections[si].name) {
            fprintf(stderr, "FSAL Error: Failed to read section name\n");
            continue;
        }
        
        uint32_t entry_count = read_u32(f);
        if (entry_count > FSAL_MAX_ENTRIES) {
            fprintf(stderr, "FSAL Error: Too many entries in section '%s': %u\n", 
                    file->sections[si].name, entry_count);
            continue;
        }
        
        file->sections[si].entries = malloc(sizeof(FSALEntry) * entry_count);
        if (!file->sections[si].entries) {
            fprintf(stderr, "FSAL Error: Failed to allocate entries for section '%s'\n", 
                    file->sections[si].name);
            continue;
        }
        
        file->sections[si].entry_count = entry_count;
        
        for (uint32_t ei = 0; ei < entry_count; ei++) {
            uint16_t key_len = read_u16(f);
            file->sections[si].entries[ei].key = read_string(f, key_len);
            
            if (!file->sections[si].entries[ei].key) {
                fprintf(stderr, "FSAL Error: Failed to read entry key\n");
                continue;
            }
            
            uint8_t type_id = read_u8(f);
            uint32_t val_size = read_u32(f);
            
            file->sections[si].entries[ei].type = (FSALType)type_id;
            
            switch (type_id) {
                case FSAL_TYPE_INT:
                    file->sections[si].entries[ei].value.i_val = read_i32(f);
                    break;
                case FSAL_TYPE_FLOAT:
                    file->sections[si].entries[ei].value.f_val = read_float(f);
                    break;
                case FSAL_TYPE_BOOL:
                    file->sections[si].entries[ei].value.b_val = read_u8(f);
                    break;
                case FSAL_TYPE_STRING:
                    file->sections[si].entries[ei].value.s_val = read_string(f, val_size);
                    break;
                case FSAL_TYPE_TABLE:
                    file->sections[si].entries[ei].value.table_val = read_table(f, val_size);
                    break;
                default:
                    fprintf(stderr, "FSAL Error: Unknown type id %u for key '%s'\n", 
                            type_id, file->sections[si].entries[ei].key);
                    fseek(f, val_size, SEEK_CUR);
                    break;
            }
        }
    }
    
    fclose(f);
    return file;
}

void fsal_free(FSALFile* file) {
    if (!file) return;
    
    for (int i = 0; i < file->link_count; i++) {
        free(file->links[i]);
    }
    free(file->links);
    
    for (int si = 0; si < file->section_count; si++) {
        free(file->sections[si].name);
        
        for (int ei = 0; ei < file->sections[si].entry_count; ei++) {
            free(file->sections[si].entries[ei].key);
            
            if (file->sections[si].entries[ei].type == FSAL_TYPE_STRING) {
                free(file->sections[si].entries[ei].value.s_val);
            } else if (file->sections[si].entries[ei].type == FSAL_TYPE_TABLE) {
                FSALTable* table = (FSALTable*)file->sections[si].entries[ei].value.table_val;
                if (table) {
                    for (int ti = 0; ti < table->entry_count; ti++) {
                        free(table->entries[ti].key);
                        if (table->entries[ti].type == FSAL_TYPE_STRING) {
                            free(table->entries[ti].value.s_val);
                        }
                    }
                    free(table->entries);
                    free(table);
                }
            }
        }
        
        free(file->sections[si].entries);
    }
    
    free(file->sections);
    free(file);
}

FSALSection* fsal_get_section(FSALFile* file, const char* section_name) {
    if (!file || !section_name) return NULL;
    
    for (int i = 0; i < file->section_count; i++) {
        if (strcmp(file->sections[i].name, section_name) == 0) {
            return &file->sections[i];
        }
    }
    
    return NULL;
}

FSALEntry* fsal_get_entry(FSALSection* section, const char* key) {
    if (!section || !key) return NULL;
    
    for (int i = 0; i < section->entry_count; i++) {
        if (strcmp(section->entries[i].key, key) == 0) {
            return &section->entries[i];
        }
    }
    
    return NULL;
}

int32_t fsal_get_int(FSALSection* section, const char* key, int32_t default_val) {
    FSALEntry* entry = fsal_get_entry(section, key);
    if (!entry) return default_val;
    
    if (entry->type != FSAL_TYPE_INT) {
        fprintf(stderr, "FSAL Warning: Key '%s' is not an integer\n", key);
        return default_val;
    }
    
    return entry->value.i_val;
}

float fsal_get_float(FSALSection* section, const char* key, float default_val) {
    FSALEntry* entry = fsal_get_entry(section, key);
    if (!entry) return default_val;
    
    if (entry->type != FSAL_TYPE_FLOAT) {
        fprintf(stderr, "FSAL Warning: Key '%s' is not a float\n", key);
        return default_val;
    }
    
    return entry->value.f_val;
}

int fsal_get_bool(FSALSection* section, const char* key, int default_val) {
    FSALEntry* entry = fsal_get_entry(section, key);
    if (!entry) return default_val;
    
    if (entry->type != FSAL_TYPE_BOOL) {
        fprintf(stderr, "FSAL Warning: Key '%s' is not a boolean\n", key);
        return default_val;
    }
    
    return entry->value.b_val;
}

const char* fsal_get_string(FSALSection* section, const char* key, const char* default_val) {
    FSALEntry* entry = fsal_get_entry(section, key);
    if (!entry) return default_val;
    
    if (entry->type != FSAL_TYPE_STRING) {
        fprintf(stderr, "FSAL Warning: Key '%s' is not a string\n", key);
        return default_val;
    }
    
    return entry->value.s_val;
}

FSALTable* fsal_get_table(FSALSection* section, const char* key) {
    FSALEntry* entry = fsal_get_entry(section, key);
    if (!entry) return NULL;
    
    if (entry->type != FSAL_TYPE_TABLE) {
        fprintf(stderr, "FSAL Warning: Key '%s' is not a table\n", key);
        return NULL;
    }
    
    return (FSALTable*)entry->value.table_val;
}

void fsal_print_file(FSALFile* file) {
    if (!file) return;
    
    printf("=== FSAL File ===\n");
    printf("Links: %d\n", file->link_count);
    for (int i = 0; i < file->link_count; i++) {
        printf("  @%s\n", file->links[i]);
    }
    
    printf("\nSections: %d\n", file->section_count);
    for (int si = 0; si < file->section_count; si++) {
        printf("\n[%s] (%d entries)\n", file->sections[si].name, file->sections[si].entry_count);
        
        for (int ei = 0; ei < file->sections[si].entry_count; ei++) {
            FSALEntry* e = &file->sections[si].entries[ei];
            printf("  %s = ", e->key);
            
            switch (e->type) {
                case FSAL_TYPE_INT:
                    printf("%d : int\n", e->value.i_val);
                    break;
                case FSAL_TYPE_FLOAT:
                    printf("%f : float\n", e->value.f_val);
                    break;
                case FSAL_TYPE_BOOL:
                    printf("%s : bool\n", e->value.b_val ? "true" : "false");
                    break;
                case FSAL_TYPE_STRING:
                    printf("\"%s\" : str\n", e->value.s_val);
                    break;
                case FSAL_TYPE_TABLE:
                    printf("[ ... ] : table\n");
                    FSALTable* table = (FSALTable*)e->value.table_val;
                    if (table) {
                        for (int ti = 0; ti < table->entry_count; ti++) {
                            printf("    %s = ", table->entries[ti].key);
                            switch (table->entries[ti].type) {
                                case FSAL_TYPE_INT:
                                    printf("%d : int\n", table->entries[ti].value.i_val);
                                    break;
                                case FSAL_TYPE_FLOAT:
                                    printf("%f : float\n", table->entries[ti].value.f_val);
                                    break;
                                case FSAL_TYPE_BOOL:
                                    printf("%s : bool\n", table->entries[ti].value.b_val ? "true" : "false");
                                    break;
                                case FSAL_TYPE_STRING:
                                    printf("\"%s\" : str\n", table->entries[ti].value.s_val);
                                    break;
                                default:
                                    printf("unknown\n");
                            }
                        }
                    }
                    break;
                default:
                    printf("unknown type\n");
            }
        }
    }
    
    printf("\n");
}
