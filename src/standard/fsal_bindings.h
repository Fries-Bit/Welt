#ifndef FSAL_BINDINGS_H
#define FSAL_BINDINGS_H

#include "vtypes.h"
#include "../fsal/fsal.h"

typedef struct {
    FSALFile* file;
    char* filepath;
} WeltFSALFile;

#define MAX_FSAL_FILES 32
WeltFSALFile loaded_fsal_files[MAX_FSAL_FILES];
int fsal_file_count = 0;

WeltValue* welt_fsal_load(const char* filepath) {
    if (fsal_file_count >= MAX_FSAL_FILES) {
        printf("Error: Maximum number of FSAL files (%d) reached\n", MAX_FSAL_FILES);
        return NULL;
    }
    
    FSALFile* file = fsal_load(filepath);
    if (!file) {
        printf("Error: Failed to load FSAL file '%s'\n", filepath);
        return NULL;
    }
    
    loaded_fsal_files[fsal_file_count].file = file;
    loaded_fsal_files[fsal_file_count].filepath = strdup(filepath);
    fsal_file_count++;
    
    WeltValue* v = wv_create();
    v->type = WT_SYS_IND;
    v->data.ptr_val = file;
    
    printf("[FSAL] Loaded file: %s (%d sections)\n", filepath, file->section_count);
    
    return v;
}

WeltValue* welt_fsal_get_string(FSALFile* file, const char* section_name, const char* key) {
    if (!file) {
        printf("Error: Invalid FSAL file pointer\n");
        return NULL;
    }
    
    FSALSection* section = fsal_get_section(file, section_name);
    if (!section) {
        printf("Error: Section '%s' not found\n", section_name);
        return NULL;
    }
    
    const char* value = fsal_get_string(section, key, NULL);
    if (!value) {
        printf("Error: Key '%s' not found in section '%s'\n", key, section_name);
        return NULL;
    }
    
    WeltValue* v = wv_create();
    v->type = WT_STRING;
    v->data.s_val = strdup(value);
    
    return v;
}

WeltValue* welt_fsal_get_int(FSALFile* file, const char* section_name, const char* key) {
    if (!file) {
        printf("Error: Invalid FSAL file pointer\n");
        return NULL;
    }
    
    FSALSection* section = fsal_get_section(file, section_name);
    if (!section) {
        printf("Error: Section '%s' not found\n", section_name);
        return NULL;
    }
    
    FSALEntry* entry = fsal_get_entry(section, key);
    if (!entry) {
        printf("Error: Key '%s' not found in section '%s'\n", key, section_name);
        return NULL;
    }
    
    WeltValue* v = wv_create();
    v->type = WT_INTEGER;
    v->data.i_val = fsal_get_int(section, key, 0);
    
    return v;
}

WeltValue* welt_fsal_get_bool(FSALFile* file, const char* section_name, const char* key) {
    if (!file) {
        printf("Error: Invalid FSAL file pointer\n");
        return NULL;
    }
    
    FSALSection* section = fsal_get_section(file, section_name);
    if (!section) {
        printf("Error: Section '%s' not found\n", section_name);
        return NULL;
    }
    
    FSALEntry* entry = fsal_get_entry(section, key);
    if (!entry) {
        printf("Error: Key '%s' not found in section '%s'\n", key, section_name);
        return NULL;
    }
    
    WeltValue* v = wv_create();
    v->type = WT_BOOL;
    v->data.b_val = fsal_get_bool(section, key, 0);
    
    return v;
}

WeltValue* welt_fsal_get_float(FSALFile* file, const char* section_name, const char* key) {
    if (!file) {
        printf("Error: Invalid FSAL file pointer\n");
        return NULL;
    }
    
    FSALSection* section = fsal_get_section(file, section_name);
    if (!section) {
        printf("Error: Section '%s' not found\n", section_name);
        return NULL;
    }
    
    FSALEntry* entry = fsal_get_entry(section, key);
    if (!entry) {
        printf("Error: Key '%s' not found in section '%s'\n", key, section_name);
        return NULL;
    }
    
    WeltValue* v = wv_create();
    v->type = WT_FLOAT;
    v->data.f_val = fsal_get_float(section, key, 0.0f);
    
    return v;
}

void welt_fsal_cleanup() {
    for (int i = 0; i < fsal_file_count; i++) {
        if (loaded_fsal_files[i].file) {
            fsal_free(loaded_fsal_files[i].file);
        }
        if (loaded_fsal_files[i].filepath) {
            free(loaded_fsal_files[i].filepath);
        }
    }
    fsal_file_count = 0;
}

#endif
