

#include "fconf.h"
#include <stdio.h>
#include <stdarg.h>

#ifndef FC_ERRMSG_SIZE
#   define FC_ERRMSG_SIZE 512
#endif

#if 0
HMODULE LoadHomeDLL(const wchar_t *DLLname)
{
	HMODULE handle;
	wchar_t *DLLpath;
	aswprintf(&DLLpath, L"%s\\%s", ts.ExeDirW, DLLname);
	handle = LoadLibraryW(DLLpath);
	free(DLLpath);
	return handle;
}
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

static void fc_form_error(fcr_t *fcr, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    form_va_err(fcr, format, args);
    va_end(args);
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
    fc_form_error(fcr, "%s : %s\n", user, err);
    msgerror_free(err);
}


static int fc_close(fcr_t *fcr, FILE *fp)
{
    fclose(fp);
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


struct fc_result *fconf_read(const char *file)
{
    struct fc_result *fcr;
    FILE *fp;
    
    fcr = (struct fc_result *)malloc(sizeof(fcr));
    if (!fcr) {
        FCTRACE("Can alloc memory\n");
        return NULL;
    }
    
    fcr->errmsg = 0;
    fcr->errsize = 0;
    fcr->status = 0;

    fp = fc_open(fcr, file);
    if (!fp)
        return fcr;

    fc_close(fcr, fp);

    return fcr;
}

void fconf_clear(struct fc_result *fcr)
{
    if (fcr->errmsg)
        free(fcr->errmsg);

    free(fcr);
}
