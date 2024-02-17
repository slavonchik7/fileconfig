

#include "fconf.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>


#ifndef FC_ERRMSG_SIZE
#   define FC_ERRMSG_SIZE 512
#endif

#ifndef FC_MAX_RAW_LINE_SIZE
#   define FC_MAX_RAW_LINE_SIZE ((FC_MAX_ATTR_VALUE_SIZE + FC_MAX_ATTR_SIZE) * 2) 
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

/* ======================== LIST SUPPORT START ======================== */

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

/* ======================== LIST SUPPORT END ======================== */

typedef struct fc_result fcr_t;

struct attr_line_data {
    char attr[FC_MAX_ATTR_SIZE];
    int asize; /* full size include '\0' symbol */
    
    char value[FC_MAX_ATTR_VALUE_SIZE];
    int vsize; /* full size include '\0' symbol */

    struct fc_sarray sarr; /* for user static using */
    struct fc_string str;  /* for user static using */

    struct list_head list;
};
typedef struct attr_line_data attrld_t;

struct fc_private {
    FILE *fp;
    char fname[256];
    struct list_head line_list;
    int linecnt;
};
typedef struct fc_private fcprvt_t;

/* defines for fast and comfortable access to fields */
#define _FCR_FILE(fcr) (((fcprvt_t*)((fcr)->__prvt))->fp)
#define _FCR_FNAME(fcr) (((fcprvt_t*)((fcr)->__prvt))->fname)
#define _FCR_LLISTP(fcr) (&(((fcprvt_t*)((fcr)->__prvt))->line_list))
#define _FCR_LINECNT(fcr) (((fcprvt_t*)((fcr)->__prvt))->linecnt)


static char *get_first_word(char *buf, unsigned int size, unsigned int *wsize);
static char *run_through_text(char *txt, unsigned int txtsize, unsigned int *wsize, unsigned int *_offset);
static inline void msgerror_free(void *ptr);
static char *msgerrors(int errcode, int *size);
static inline char *msgerror(int errcode);
static inline char *lmsgerror();
static char *form_va_msg(int *ssave, const char *format, va_list args);
static inline void form_va_err(fcr_t *fcr, const char *format, va_list args);
static void fc_form_error(fcr_t *fcr, int errcode, const char *format, ...);
static void fc_form_sys_error(fcr_t *fcr, const char *format, ...);
static inline int fc_close(fcr_t *fcr);
static FILE *fc_open(fcr_t *fcr, const char *fpath);
static void fc_clear_raw_line_list(struct fc_result *fcr);
static int fc_parse_good_line(struct fc_result *fcr, char *line, int size);
static attrld_t *fc_add_line(struct fc_result *fcr, char *line, int size);
static int fc_line_parse(struct fc_result *fcr);
static int fc_full_parse(struct fc_result *fcr);
struct fc_result *fconf_read(const char *file);
void fconf_clear(struct fc_result *fcr);


/* save first word in string */
static char *get_first_word(char *buf, unsigned int size, unsigned int *wsize) 
{
    const char sIN = 1;
    const char sOUT = 0;
    unsigned int wlen = *wsize = 0;
    char state = sOUT;
    char *pword; 

    for (unsigned int i = 0; i < size; i++) {
        char b = *buf;

        if (b == ' ' || b == '\0' || b == '\n' || b == '\t') {
            if (state == sIN)
                goto ret_ffound;
            state = sOUT;
        } else if (state == sOUT) {
            pword = buf;
            state = sIN;
        }

        if (state == sIN)
            wlen++;

        buf++;
    }

    if (state == sIN) {
    ret_ffound:
            *wsize = wlen;
            return pword; 
    }

    return NULL;
}

/* @brief function allows you to run through all the words in a line, 
 *      save a pointer to the first character of the word and its length 
 *  @param txt      pointer to the first character of the string 
 *  @param txtsize  the length of the txt string, including the end of line character 
 *  @param wsize    a pointer to save the size of the current returned word, without the end of the line 
 *  @param _offset  the parameter retains the offset in the string until the end of the current word  
 *  ATTENTION!! all arguments except wsize should not be changed when passing them to the function at each iteration 
 * @return
 *      pointer on word - success
 *      NULL pointer    - error */
static char *run_through_text(char *txt, unsigned int txtsize, unsigned int *wsize, unsigned int *_offset) 
{
    unsigned int offset = *_offset;
    char *w;

    if (offset >= txtsize)
        return NULL;

    w = get_first_word(txt + offset, txtsize - offset, wsize);
    
    if (w)
        offset = w - txt + *wsize; /* offset to the beginning of the current word */

    return w;
}

/* clear allocated error's buffer */
static inline void msgerror_free(void *ptr)
{
#ifdef _WIN32
    LocalFree((HLOCAL)ptr);
#else
    free(ptr);
#endif
}

/* get error string from system and alloc buffer for it */
static char *msgerrors(int errcode, int *size)
{
    char *buf;
#ifdef _WIN32
    if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            0, 
            errcode, 
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), 
            (LPSTR)&buf,
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

/* va_args formating function with save result size*/
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

/* format and save to fcr error's buffer */
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

/* form error with syste, error message */
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


static inline int fc_close(fcr_t *fcr)
{
    return fclose(((fcprvt_t*)(fcr->__prvt))->fp);
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

static char *fc_alloc_string(struct fc_string *fcs, int len /* without '\0' */, char *init)
{
    fcs->s = (char *)malloc(len + 1);
    if (!fcs->s)
        return NULL;

    if (init)
        memcpy(fcs->s, init, len + 1);
    else
        memset(fcs->s, 0, len + 1);

    fcs->len = len;
    
    return fcs->s;
}

static inline char *fc_alloc_string_zero(struct fc_string *fcs, int len /* without '\0' */)
{
    return fc_alloc_string(fcs, len, NULL);
}

static int fc_alloc_sarray(struct fc_sarray *fcsa, int size)
{
    fcsa->arr = (struct fc_string *)malloc(size * sizeof(struct fc_string));
    if (!fcsa->arr)
        return -1;
    
    fcsa->size = size;
    
    while(size) {
        --size;
        fcsa->arr[size] = FC_STRING_INIT;
    }

    return 0;
}

static void fc_clear_raw_line_list(struct fc_result *fcr)
{
    if (!list_empty(_FCR_LLISTP(fcr)))
        list_free_items(_FCR_LLISTP(fcr), attrld_t, list, free);

    INIT_LIST_HEAD(_FCR_LLISTP(fcr));
}

static attrld_t *fc_add_line(struct fc_result *fcr, char *line, int size)
{
    attrld_t *rl;
    char *s;
    unsigned int asize;
    unsigned int vsize;

    s = get_first_word(line, size, &asize);
    //FCTRACE("first:%s\n", s);

    if (asize + 1 > FC_MAX_ATTR_SIZE) {
        fc_form_error(fcr, FCONF_ELONGATTR, "long attr name, should be less than %d\n", FC_MAX_ATTR_SIZE - 1);
        return NULL;
    }

    rl = (attrld_t*)malloc(sizeof(*rl));
    if (!rl) {
        fc_form_sys_error(fcr, "can not alloc memory\n");
        return NULL;
    }

    /* copy attribute */
    rl->asize = asize;
    memcpy(rl->attr, s, asize);
    rl->attr[asize] = 0;

    //FCTRACE("copy success:%s\n", rl->attr);

    /* copy value */
    s += asize + 1; /* value pointer */
    rl->vsize = (size - (int)(s - line)); /* get value size */
    //FCTRACE("vsize:%d, vstr:%s\n", rl->vsize, s);
    //s[rl->vsize] = 0;
    
    memcpy(rl->value, s, rl->vsize);

    rl->sarr = FC_SARRAY_INIT;
    rl->str = FC_STRING_INIT;

    INIT_LIST_HEAD(&rl->list);
    list_add_tail(&rl->list, _FCR_LLISTP(fcr));

    _FCR_LINECNT(fcr)++;

    //fc_parse_good_line(fcr, line, size); /* we found the whole line */
    static int cnt = 0;
    FCTRACE("%d:%s:%s\n", cnt++, rl->attr, rl->value);
    return rl;
}

static int fc_parse_good_line(struct fc_result *fcr, char *line, int size) 
{
    static int cnt = 0;
    printf("%d:%d:%s\n", cnt++, size, line);
    return 0;
}

/* the primary viewing of the file
 * is the main line division processing of the character '\'
 * the names of attributes are not highlighted
 * here all the received lines are saved to a list in the fcr_t structure in the __prvt field 
 * @return 
 *  0   - success
 *  -1  - error */
static int fc_line_parse(struct fc_result *fcr) 
{
    char buf[FC_MAX_RAW_LINE_SIZE]; /* for template saving string */
    char bufsave[FC_MAX_RAW_LINE_SIZE]; /* for error indicate string */
    int save_cnt = 0;
    int save_pos_err;
    int cnt = 0;
    int line_cnt = 1;
    char block[512];
    int nrd;
    bool was_bslash = false;
    bool last_bslash = false;
    bool last_double_bslash = false;
    bool line_error = false;
    bool line_not_empty = false;
    FILE *fp = _FCR_FILE(fcr);

    while(1) {
        nrd = fread(block, 1, sizeof(block), fp);
        FCTRACE("nrd:%d\n", nrd);
        
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
                            if (line_not_empty) {
                                if (!fc_add_line(fcr, buf, cnt)) {
                                    fc_clear_raw_line_list(fcr); /* full clear line list */
                                    return -1;
                                }
                            }
                            cnt = 0;
                            line_cnt++;
                            line_not_empty = false;
                        }
                    }
                } else {
                    if (block[i] != ' ')
                        line_not_empty = true;
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
                if (!fc_add_line(fcr, buf, cnt)) {
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
    fcprvt_t *pvt = (fcprvt_t *)fcr->__prvt;
    attrld_t *pos;

    if (fc_line_parse(fcr) < 0)
        return -1;

    return 0;
#if 0
    list_for_each_entry(pos, &pvt->line_list, list) {
        pos->uattr = (typeof(*pos->uattr)*)malloc(sizeof(*(pos->uattr)));
        if (!pos->uattr)
            goto err_out;
        
        pos->uattr->
    }

    return 0;

err_out:
    list_for_each_entry(pos, &pvt->line_list, list) {
        if(pos->uattr)
            free(pos->uattr);
    }
#endif
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
