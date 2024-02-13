
#include "fconf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


int main(int argc, char **argv)
{
    struct fc_result *fcr = fconf_read("fconf.h");
    if (!fcr) {
        printf("fatal alloc\n");
        exit(1);
    }
    
    if (FCISERR(fcr)) {
        printf(fcr->errmsg);
        fconf_clear(fcr);
        exit(1);
    }
    printf("fcr OK\n");

    fconf_clear(fcr);
    exit(0);
}