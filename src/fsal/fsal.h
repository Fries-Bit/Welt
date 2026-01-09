#ifndef FSAL_H
#define FSAL_H

#include <stdint.h>
#include <stdio.h>

#define FSAL_MAGIC "ATFF"
#define FSAL_MAX_SECTIONS 128
#define FSAL_MAX_ENTRIES 256
#define FSAL_MAX_TABLE_ENTRIES 128
#define FSAL_MAX_STRING_LEN 1024

typedef enum {
    FSAL_TYPE_INT = 1,
    FSAL_TYPE_FLOAT = 2,
    FSAL_TYPE_BOOL = 3,
    FSAL_TYPE_STRING = 4,
    FSAL_TYPE_TABLE = 5
} FSALType;

typedef struct {
    char* key;
    FSALType type;
    union {
        int32_t i_val;
        float f_val;
        int b_val;
        char* s_val;
        void* table_val;
    } value;
} FSALEntry;

typedef struct {
    FSALEntry* entries;
    int entry_count;
} FSALTable;

typedef struct {
    char* name;
    FSALEntry* entries;
    int entry_count;
} FSALSection;

typedef struct {
    char** links;
    int link_count;
    FSALSection* sections;
    int section_count;
} FSALFile;

FSALFile* fsal_load(const char* filepath);
void fsal_free(FSALFile* file);

FSALSection* fsal_get_section(FSALFile* file, const char* section_name);
FSALEntry* fsal_get_entry(FSALSection* section, const char* key);

int32_t fsal_get_int(FSALSection* section, const char* key, int32_t default_val);
float fsal_get_float(FSALSection* section, const char* key, float default_val);
int fsal_get_bool(FSALSection* section, const char* key, int default_val);
const char* fsal_get_string(FSALSection* section, const char* key, const char* default_val);
FSALTable* fsal_get_table(FSALSection* section, const char* key);

void fsal_print_file(FSALFile* file);

#endif
