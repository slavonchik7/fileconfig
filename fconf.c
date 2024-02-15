

#include "fconf.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>


#ifndef FC_ERRMSG_SIZE
#   define FC_ERRMSG_SIZE 512
#endif

#ifndef FC_MAX_RAW_LINE_LEN
#   define FC_MAX_RAW_LINE_LEN ((FC_MAX_ATTR_VALUE_LEN + FC_MAX_ATTR_LEN) * 2) 
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

/* ======================== LIST SUPPORT ======================== */

#ifdef  offsetof
#	undef 	offsetof
#endif
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
    
#define LIST_POISON1  ((void *) 0x00100100)
#define LIST_POISON2  ((void *) 0x00200200)

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) ((struct list_head){ &(name), &(name) })
#define LIST_HEAD_INIT_PTR(ptr) ((struct list_head){ (ptr), (ptr) })

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_free_items(head, type, member, ffree) 			\
	do {													\
		struct list_head *__lcl_pos = (head)->next;			\
    	struct list_head *__lcl_save;						\
		while (__lcl_pos != (head)) {						\
			__lcl_save = __lcl_pos;							\
			__lcl_pos = __lcl_pos->next;					\
			list_del(__lcl_save);							\
			(ffree)(container_of(__lcl_save, type, member));\
		}													\
	} while (0)

#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head);	\
       pos = pos->next)

#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/* ======================== LIST SUPPORT ======================== */

typedef struct fc_result fcr_t;

struct raw_line {
    char line[FC_MAX_RAW_LINE_LEN];
    int size; /* full size include '\0' symbol */
    struct list_head list;
};
typedef struct raw_line rawl_t;

struct fc_private {
    FILE *fp;
    char fname[256];
    struct list_head line_list;
    int linecnt;
};
typedef struct fc_private fcprvt_t;


#define _FCR_FILE(fcr) (((fcprvt_t*)((fcr)->__prvt))->fp)
#define _FCR_FNAME(fcr) (((fcprvt_t*)((fcr)->__prvt))->fname)
#define _FCR_LLISTP(fcr) (&(((fcprvt_t*)((fcr)->__prvt))->line_list))
#define _FCR_LINECNT(fcr) (((fcprvt_t*)((fcr)->__prvt))->linecnt)


static inline void msgerror_free(void *ptr);
static char *msgerrors(int errcode, int *size);
static inline char *msgerror(int errcode);
static inline char *lmsgerror();
static char *form_va_msg(int *ssave, const char *format, va_list args);
static inline void form_va_err(fcr_t *fcr, const char *format, va_list args);
static void fc_form_error(fcr_t *fcr, int errcode, const char *format, ...);
static void fc_form_sys_error(fcr_t *fcr, const char *format, ...);
static int fc_close(fcr_t *fcr);
static FILE *fc_open(fcr_t *fcr, const char *fpath);
static void fc_clear_raw_line_list(struct fc_result *fcr);
static int fc_parse_good_line(struct fc_result *fcr, char *line, int size);
static rawl_t *fc_add_raw_line(struct fc_result *fcr, char *line, int size);
static int fc_line_parse(struct fc_result *fcr);
static int fc_full_parse(struct fc_result *fcr);
struct fc_result *fconf_read(const char *file);
void fconf_clear(struct fc_result *fcr);

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

static void fc_clear_raw_line_list(struct fc_result *fcr)
{
    if (!list_empty(_FCR_LLISTP(fcr)))
        list_free_items(_FCR_LLISTP(fcr), rawl_t, list, free);

    INIT_LIST_HEAD(_FCR_LLISTP(fcr));
}

static rawl_t *fc_add_raw_line(struct fc_result *fcr, char *line, int size)
{
    rawl_t *rl;
    
    rl = (rawl_t*)malloc(sizeof(*rl));
    if (!rl) {
        fc_form_sys_error(fcr, "can not alloc memory\n");
        return NULL;
    }
    
    memcpy(rl->line, line, size);
    rl->size = size;

    INIT_LIST_HEAD(&rl->list);
    list_add_tail(&rl->list, _FCR_LLISTP(fcr));

    _FCR_LINECNT(fcr)++;

    fc_parse_good_line(fcr, line, size); /* we found the whole line */

    return rl;
}

static int fc_parse_good_line(struct fc_result *fcr, char *line, int size) 
{
    static int cnt = 0;
    printf("%d:%d:%s\n", cnt++, size, line);
}

static int fc_line_parse(struct fc_result *fcr) 
{
    char buf[FC_MAX_RAW_LINE_LEN]; /* for template saving string */
    char bufsave[FC_MAX_RAW_LINE_LEN]; /* for template saving string */
    int save_line_cnt = 1;
    int save_cnt = 0;
    int save_pos_err;
    int cnt = 0;
    int line_cnt = 1;
    char block[32];
    int nrd;
    bool was_bslash = false;
    bool last_bslash = false;
    bool last_double_bslash = false;
    bool line_error = false;
    FILE *fp = _FCR_FILE(fcr);

    while(1) {
        nrd = fread(block, 1, sizeof(block), fp);
        
        if (nrd < sizeof(block) && ferror(fp)) {
            FCTRACE("err fread\n");
            fc_form_sys_error(fcr, "error fread");
            return -1;
        }

        for (int i = 0; i < nrd; i++) {
            /* save real line */
            bufsave[save_cnt++] = block[i];

            if (block[i] != '\\') {

                if (last_bslash) {
                    if (block[i] != '\n') {
                        line_error = true;
                        save_pos_err = save_cnt;
                    }
                    last_bslash = false;
                }

                if (block[i] == '\n') {
                    bufsave[save_cnt - 1] = 0;
                    save_cnt = 0;

                    if (was_bslash) {
                        was_bslash = false;
                        continue;
                    } else {
                        buf[cnt++] = 0; /* end of string */

                        if (line_error) {
                            FCTRACE("error parsing line:%d, pos:%d, symb:'\\', line:\"%s\"\n", line_cnt, save_pos_err, bufsave);
                            fc_form_error(fcr, FCONF_EPARSE, "error parsing line:%d, pos:%d, symb:'\\', line:\"%s\"\n", line_cnt, save_pos_err, bufsave);
                            line_error = false;
                            return -1;
                        } else {
                            if (!fc_add_raw_line(fcr, buf, cnt)) {
                                fc_clear_raw_line_list(fcr); /* full clear line list */
                                return -1;
                            }
                            cnt = 0;
                            line_cnt++;
                        }
                    }
                } else {
                    buf[cnt++] = block[i];
                    was_bslash = false;
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
                if (!fc_add_raw_line(fcr, buf, cnt)) {
                    fc_clear_raw_line_list(fcr); /* full clear line list */
                    return -1;
                }
            }
            break;
        }
    }

    return 0;
}
static int fc_full_parse(struct fc_result *fcr)
{
    return fc_line_parse(fcr);
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

    memcpy(fcp->fname, file, strlen(file) + 1);
    fcp->fp = fp;
    fcp->linecnt = 0;
    INIT_LIST_HEAD(&fcp->line_list);
    
    fcr->__prvt = (void *)fcp;

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

    fc_clear_raw_line_list(fcr);

    free(fcr);
}
