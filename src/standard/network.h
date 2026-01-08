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
    // Since writing a raw HTTP client in C is 1000+ lines,
    // We will use a system call to curl for the interpreter prototype.
    
    printf("Fetching content from: %s\n", url);
    
    char cmd[512];
    sprintf(cmd, "curl -s \"%s\" > .welt_tmp_net", url);
    int res = system(cmd);
    
    if (res == 0) {
        FILE* f = fopen(".welt_tmp_net", "r");
        if(f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            char *string = malloc(fsize + 1);
            fread(string, 1, fsize, f);
            fclose(f);
            
            string[fsize] = 0;
            
            output_var->type = WT_STRING;
            output_var->data.s_val = string;
            
            // Cleanup
            remove(".welt_tmp_net");
        }
    } else {
        printf("Error: Network request failed.\n");
    }
}

#endif