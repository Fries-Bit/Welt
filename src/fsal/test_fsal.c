#include <stdio.h>
#include "fsal.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <file.atff>\n", argv[0]);
        return 1;
    }
    
    printf("Loading FSAL file: %s\n\n", argv[1]);
    
    FSALFile* file = fsal_load(argv[1]);
    if (!file) {
        fprintf(stderr, "Failed to load FSAL file\n");
        return 1;
    }
    
    printf("=== File loaded successfully ===\n\n");
    
    fsal_print_file(file);
    
    printf("=== Accessing specific values ===\n");
    
    FSALSection* app = fsal_get_section(file, "app");
    if (app) {
        const char* name = fsal_get_string(app, "name", "Unknown");
        int32_t version = fsal_get_int(app, "version", 0);
        int enabled = fsal_get_bool(app, "enabled", 0);
        float pi = fsal_get_float(app, "pi_value", 0.0f);
        
        printf("[app]\n");
        printf("  name: %s\n", name);
        printf("  version: %d\n", version);
        printf("  enabled: %s\n", enabled ? "true" : "false");
        printf("  pi_value: %f\n", pi);
        printf("\n");
    }
    
    FSALSection* database = fsal_get_section(file, "database");
    if (database) {
        const char* host = fsal_get_string(database, "host", "localhost");
        int32_t port = fsal_get_int(database, "port", 5432);
        int use_ssl = fsal_get_bool(database, "use_ssl", 0);
        float timeout = fsal_get_float(database, "timeout", 30.0f);
        
        printf("[database]\n");
        printf("  host: %s\n", host);
        printf("  port: %d\n", port);
        printf("  use_ssl: %s\n", use_ssl ? "true" : "false");
        printf("  timeout: %.1f\n", timeout);
        printf("\n");
    }
    
    FSALSection* settings = fsal_get_section(file, "settings");
    if (settings) {
        FSALTable* config = fsal_get_table(settings, "config");
        if (config) {
            printf("[settings.config]\n");
            for (int i = 0; i < config->entry_count; i++) {
                FSALEntry* e = &config->entries[i];
                printf("  %s = ", e->key);
                switch (e->type) {
                    case FSAL_TYPE_INT:
                        printf("%d\n", e->value.i_val);
                        break;
                    case FSAL_TYPE_FLOAT:
                        printf("%f\n", e->value.f_val);
                        break;
                    case FSAL_TYPE_BOOL:
                        printf("%s\n", e->value.b_val ? "true" : "false");
                        break;
                    case FSAL_TYPE_STRING:
                        printf("%s\n", e->value.s_val);
                        break;
                    default:
                        printf("unknown\n");
                }
            }
            printf("\n");
        }
    }
    
    fsal_free(file);
    
    printf("=== Test completed successfully ===\n");
    
    return 0;
}
