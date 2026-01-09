#ifndef NETWORK_H
#define NETWORK_H

#include "vtypes.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Network Operations
 * fwww_at(url, varname);
 */

void fwww_at(const char* url, WeltValue* output_var) {
    if (!url || !output_var) {
        printf("Error: Invalid parameters to fwww_at\n");
        return;
    }
    
    printf("Fetching content from: %s\n", url);
    
    char cmd[512];
    int written = snprintf(cmd, sizeof(cmd), "curl -s \"%s\" > .welt_tmp_net", url);
    if (written < 0 || written >= sizeof(cmd)) {
        printf("Error: URL too long or formatting error\n");
        return;
    }
    
    int res = system(cmd);
    
    if (res == 0) {
        FILE* f = fopen(".welt_tmp_net", "r");
        if(!f) {
            printf("Error: Could not open temporary file\n");
            remove(".welt_tmp_net");
            return;
        }
        
        if (fseek(f, 0, SEEK_END) != 0) {
            printf("Error: Could not seek to end of file\n");
            fclose(f);
            remove(".welt_tmp_net");
            return;
        }
        
        long fsize = ftell(f);
        if (fsize < 0) {
            printf("Error: Could not determine file size\n");
            fclose(f);
            remove(".welt_tmp_net");
            return;
        }
        
        if (fseek(f, 0, SEEK_SET) != 0) {
            printf("Error: Could not seek to beginning of file\n");
            fclose(f);
            remove(".welt_tmp_net");
            return;
        }
        
        char *string = malloc(fsize + 1);
        if (!string) {
            printf("Error: Memory allocation failed\n");
            fclose(f);
            remove(".welt_tmp_net");
            return;
        }
        
        size_t read_bytes = fread(string, 1, fsize, f);
        fclose(f);
        
        if (read_bytes != (size_t)fsize) {
            printf("Error: Could not read entire file\n");
            free(string);
            remove(".welt_tmp_net");
            return;
        }
        
        string[fsize] = 0;
        
        output_var->type = WT_STRING;
        output_var->data.s_val = string;
        
        remove(".welt_tmp_net");
    } else {
        printf("Error: Network request failed with code %d\n", res);
        output_var->type = WT_SS_NULL;
    }
}

#endif