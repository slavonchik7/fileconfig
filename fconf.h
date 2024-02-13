#ifndef __FCONF_H__
#define __FCONF_H__





#define __user_api

struct fc_result {
#define FCONF_OK    0x00
#define FCONF_ERR   0x01
    int status;

/* string msg about last error */
    char *errmsg;
    int errsize;
};


#define FCISERR(fcr) ((fcr)->status == FCONF_ERR)


struct fc_result  __user_api *fconf_read(const char *file);
void              __user_api fconf_clear(struct fc_result *fcr);

#undef __user_api
#endif /* __FCONF_H__ */