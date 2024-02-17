#ifndef __FCONF_H__
#define __FCONF_H__




#define __user_api


#ifndef FC_MAX_ATTR_VALUE_SIZE
#   define FC_MAX_ATTR_VALUE_SIZE   128
#endif

#ifndef FC_MAX_ATTR_SIZE
#   define FC_MAX_ATTR_SIZE         32
#endif


struct fc_sarray {
    struct fc_string *arr;
    int size;
};

struct fc_string {
    char *s;
    int len;
};

#define FC_SARRAY_INIT ((struct fc_sarray){0,0})
#define FC_STRING_INIT ((struct fc_string){0,0})


struct fc_attr {
    char attr[FC_MAX_ATTR_SIZE];
    int alen;

    char value[FC_MAX_ATTR_VALUE_SIZE];
    int vlen;

/* for internal work, dont touch!! */
    void *__prvt;
};

struct fc_result {
#define FCONF_OK        0x00
#define FCONF_ERR       0x01
#define FCONF_EPARSE    0x02
#define FCONF_ELONGATTR 0x03
    int status;

/* string msg about last error */
    char *errmsg;
    int errsize;

/* for internal work, dont touch!! */
    void *__prvt;
};

#define FCISERR(fcr) ((fcr)->status != FCONF_OK)


extern struct fc_result  __user_api *fconf_read(const char *file);
extern void              __user_api fconf_clear(struct fc_result *fcr);

#undef __user_api
#endif /* __FCONF_H__ */