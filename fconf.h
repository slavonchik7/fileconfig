#ifndef __FCONF_H__
#define __FCONF_H__


#define __user_api


#ifndef FC_MAX_ATTR_VALUE_SIZE
#   define FC_MAX_ATTR_VALUE_SIZE   512
#endif

#ifndef FC_MAX_ATTR_SIZE
#   define FC_MAX_ATTR_SIZE         128
#endif


struct fc_string {
    char *s;
    int len;
};

struct fc_sarray {
    struct fc_string *arr;
    int size;
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

#define FCONF_FSTATIC    0x01
#define FCONF_FALLOC     0x02

struct fc_result {
#define FCONF_OK        0x00
#define FCONF_ERR       0x01
#define FCONF_EPARSE    0x02
#define FCONF_ELONGATTR 0x03
#define FCONF_EBADFLAGS 0x04
#define FCONF_ENOMATCH  0x05
    int status;

/* string msg about last error */
    char *errmsg;
    int errsize;

/* for internal work, dont touch!! */
    void *__prvt;
};

#define FCISERR(fcr) ((fcr)->status != FCONF_OK)


extern void              __user_api fc_sarray_free(struct fc_sarray *fcsa);
extern void              __user_api fc_string_free(struct fc_string *fcs);
extern struct fc_result  __user_api *fconf_read(const char *file);
extern void              __user_api fconf_clear(struct fc_result *fcr);
extern struct fc_string  __user_api *fconf_attr_string(struct fc_result *fcr, char *attr, char flags);
extern struct fc_sarray  __user_api *fconf_attr_sarray(struct fc_result *fcr, char *attr, char flags);


#undef __user_api
#endif /* __FCONF_H__ */