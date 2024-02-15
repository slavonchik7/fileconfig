#ifndef __FCONF_H__
#define __FCONF_H__





#define __user_api

#define FC_MAX_ATTR_VALUE_LEN   512
#define FC_MAX_ATTR_LEN         128

struct fc_result {
#define FCONF_OK    0x00
#define FCONF_ERR   0x01
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