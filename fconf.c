

#include "fconf.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>


#ifndef FC_ERRMSG_SIZE
#   define FC_ERRMSG_SIZE 512
#endif


#define FCDEBUG 1

#if FCDEBUG
#   define FCTRACE(...) \
    do { \
        printf(__VA_ARGS__); \
        fflush(stdout);  \
    } while (0)
#else
#   define FCTRACE(...)
#endif

#ifdef _WIN32
#   include <errhandlingapi.h>
#   include <windows.h>

#   define FCERRNO ((int)GetLastError())
#else
#   include <errno.h>

#   define FCERRNO (errno)
#endif



typedef struct fc_result fcr_t;

struct fc_private {
    FILE *fp;
    char fname[256];
};

typedef struct fc_private fcprvt_t;


#define _FCR_FILE(fcr) (((fcprvt_t*)((fcr)->__prvt))->fp)
#define _FCR_FNAME(fcr) (((fcprvt_t*)((fcr)->__prvt))->fname)



static inline void msgerror_free(void *ptr)
{
#ifdef _WIN32
    LocalFree((HLOCAL)ptr);
#else
    free(ptr);
#endif
}

static char *msgerrors(int errcode, int *size)
{
    char *buf;
#ifdef _WIN32
    if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            0, 
            errcode, 
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), 
            &buf, 
            0, 
            0)) return NULL;
    if (size && buf)
        *size = (int)strlen(buf) + 1;

#else
    char *p = strerror(errcode);
    int len = (int)strlen(p);
    buf = (char *)malloc(len + 1);
    if (!buf)
        return NULL;
    memcpy(buf, p, len + 1);
#endif
    return buf;
}

static inline char *msgerror(int errcode)
{
    return msgerrors(errcode, NULL);
}

static inline char *lmsgerror()
{
    return msgerrors(FCERRNO, NULL);
}

static char *form_va_msg(int *ssave, const char *format, va_list args)
{
    char temp[FC_ERRMSG_SIZE];
    char *msg;
    int len; 

    len = vsnprintf(temp, sizeof(temp), format, args);

    msg = (char *)malloc(len+1);
    if (!msg)
        return NULL;

    memcpy(msg, temp, len+1);

    if (ssave)
        *ssave = len + 1;

    return msg;
}

static inline void form_va_err(fcr_t *fcr, const char *format, va_list args)
{  
    if (fcr->errmsg)
        free(fcr->errmsg);

    fcr->errsize = 0;
    fcr->errmsg = form_va_msg(&fcr->errsize, format, args);
}

static void fc_form_error(fcr_t *fcr, int errcode, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    form_va_err(fcr, format, args);
    va_end(args);
    fcr->status = errcode;
}

static void fc_form_sys_error(fcr_t *fcr, const char *format, ...)
{
    char user[FC_ERRMSG_SIZE];
    char *err;
    va_list args;

    va_start(args, format);
    vsnprintf(user, sizeof(user), format, args);
    va_end(args);

    err = lmsgerror();
    fc_form_error(fcr, FCERRNO, "%s : %s\n", user, err);
    msgerror_free(err);
}


static int fc_close(fcr_t *fcr)
{
    fclose(((fcprvt_t*)(fcr->__prvt))->fp);
}

static FILE *fc_open(fcr_t *fcr, const char *fpath)
{
    FILE *fp;
    
    if ((fp = fopen(fpath, "r")) == NULL) {
        fc_form_sys_error(fcr, "Can not open %s", fpath);
        fcr->status = FCONF_ERR;
        return NULL;
    }

    return fp;
}

static int fc_parse_good_line(struct fc_result *fcr, char *line, int size) {
    static int cnt = 0;
    printf("%d:%d:%s\n", cnt++, size, line);
}

static int fc_full_parse(struct fc_result *fcr)
{
    char ch;
    char last_blck_ch = 0;
    char buf[(FC_MAX_ATTR_VALUE_LEN + FC_MAX_ATTR_LEN) * 2]; /* for template saving string */
    int cnt = 0;
    char block[32];
    FILE *fp = _FCR_FILE(fcr);
    int nrd;
    bool was_double_bslash = false;
    bool was_bslash = false;
    int bslash_cnt = 0;
    bool last_bslash = false;
    bool last_double_bslash = false;
    bool was_line_bslash = false;

    while(1) {
        
        //FCTRACE("ntrhthr\n");
        nrd = fread(block, 1, sizeof(block), fp);
        //FCTRACE("nrd:%d\n", nrd);
        
        if (nrd < sizeof(block) && ferror(fp)) {
            FCTRACE("err fread\n");
            fc_form_sys_error(fcr, "error fread");
            return -1;
        }

        for (int i = 0; i < nrd; i++) {

            if (block[i] != '\\') {
                if (was_bslash)
                    was_line_bslash = true;

                if (last_bslash)
                    last_bslash = false;

                if (block[i] == '\n') {
                    if (was_bslash || was_line_bslash) {
                        was_bslash = false;
                        was_line_bslash = false;
                        continue;
                    } else {
                        buf[cnt++] = 0; /* end of string */
                        fc_parse_good_line(fcr, buf, cnt); /* we found the whole line */
                        cnt = 0;
                    }
                } else {
                    buf[cnt++] = block[i];
                    was_bslash = false;
                    was_line_bslash = false;
                }
            } else {
                if (last_bslash) {
                    if (last_double_bslash) {
                        last_double_bslash = false;
                        was_bslash = true;
                    } else {
                        last_double_bslash = true;
                        buf[cnt++] = '\\';
                        was_bslash = false;
                    }
                } else {
                    last_bslash = true;
                    was_bslash = true;
                }
            }

        }

        if (nrd < sizeof(block)) {
            /* checking the end of the file will be unnecessary, since it was previously checked that there is no error  */
            if (cnt != 0) {
                buf[cnt++] = 0; /* end of file and also string */
                fc_parse_good_line(fcr, buf, cnt); /* we found the whole line */
            }
            break;
        }
    }

    return 0;
}

struct fc_result *fconf_read(const char *file)
{
    fcr_t *fcr;
    fcprvt_t *fcp;
    FILE *fp;
    
    fcr = (fcr_t*)malloc(sizeof(*fcr));
    if (!fcr) {
        FCTRACE("Can not alloc memory\n");
        return NULL;
    }
    
    fcr->errmsg = 0;
    fcr->errsize = 0;
    fcr->status = 0;

    fp = fc_open(fcr, file);
    if (!fp)
        return fcr;

    fcp = malloc(sizeof(fcprvt_t));
    if (!fcp) {
        FCTRACE("Can not alloc memory\n");
        fc_form_sys_error(fcr, "can not alloc memory\n");
        fc_close(fcr);
        return fcr;
    }
    FCTRACE("ok fcp alloc\n");
    memcpy(fcp->fname, file, strlen(file) + 1);
    fcp->fp = fp;
    fcr->__prvt = (void *)fcp;
FCTRACE("ok init fcp\n");
FCTRACE("start parse\n");
    if (fc_full_parse(fcr) < 0) {
        fc_close(fcr);
        return fcr;
    }

    FCTRACE("end parse\n");
    /* template for debug */
    fc_close(fcr);

    return fcr;
}

void fconf_clear(struct fc_result *fcr)
{
    if (fcr->errmsg)
        free(fcr->errmsg);

    free(fcr);
}
