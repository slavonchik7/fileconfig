
#include "fconf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <time.h>

static struct fc_result *fcr;

static int test_smatch(char *attr)
{
    static int cnt = 0;

    printf("=========== test string %d start ==========\n", cnt);
    printf("attr:%s\n", attr);
    struct fc_string *s = fconf_attr_string(fcr, attr, FCONF_FSTATIC);
    if (!s) {
        printf(fcr->errmsg);
        printf("=========== test string %d finish ==========\n\n\n", cnt);
        cnt++;
        return -1;
    }
    printf("%d|%s\n", s->len, s->s);
    //fc_string_free(s);
    printf("=========== test string %d finish ==========\n\n\n", cnt);
    fflush(stdout);
    cnt++;
    return 0;
}


static int test_amatch(char *attr)
{
    static int cnt = 0;

    printf("=========== test array %d start ==========\n", cnt);
    printf("attr:%s\n", attr);
    struct fc_sarray *s = fconf_attr_sarray(fcr, attr, FCONF_FSTATIC);
    if (!s) {
        printf(fcr->errmsg);
        printf("=========== test array %d finish ==========\n\n\n", cnt);
        cnt++;
        return -1;
    }
    for (int i = 0; i < s->size; i++) {
        printf("\t%d:%d|%s\n", i, s->arr[i].len, s->arr[i].s);
    }
    //fc_sarray_free(s);
    printf("=========== test array %d finish ==========\n\n\n", cnt);
    fflush(stdout);
    cnt++;
    return 0;
}

int main(int argc, char **argv)
{
    
    fcr = fconf_read("test.txt");
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

#if 0
    test_smatch("attr1");
    test_smatch("123");
    test_smatch("badattr");
    test_smatch("name");

    test_amatch("attr1");
    test_amatch("123");
    test_amatch("badattr");
    test_amatch("name");
#endif
    Sleep(100000);
    fconf_clear(fcr);



    exit(0);
}