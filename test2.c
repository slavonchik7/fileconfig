
#include "fconf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


int main(int argc, char **argv)
{
    struct fc_result *fcr = fconf_read("test.txt");
    if (!fcr) {
        printf("fatal alloc\n");
        exit(1);
    }
    
    if (FCISERR(fcr)) {
        printf(fcr->errmsg);
        printf("status:%d\n", fcr->status);
        fconf_clear(fcr);
        exit(1);
    }
    printf("fcr OK\n");

    fconf_clear(fcr);
    exit(0);
}