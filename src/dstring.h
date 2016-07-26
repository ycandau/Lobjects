#ifndef YC_DSTRING_H_
#define YC_DSTRING_H_

/****************************************************************
*  Header files
*/
#include <string.h>

/****************************************************************
*  Typedef and type sizes
*/
typedef struct _dstring_struct t_dstr_struct;
typedef struct _dstring_struct * t_dstr;

typedef unsigned __int32 t_dstr_int; 
#define DSTR_INT_SIZE 32

#define DSTR_LEN_ERR    ((t_dstr_int)-1)
#define DSTR_LEN_MAX    ((t_dstr_int)-2)  // to avoid overflow on +1 and reserve ()-1 for errors
#define DSTR_LEN_NTOA   22
#define DSTR_LEN_PRINTF 10               // used to try a fixed buffer first

/****************************************************************
*  Extern variables declarations
*/
extern char _null_cstr[];
extern t_dstr_struct _null_dstr_struct;

/****************************************************************
*  Exposed preprocessor macros
*/
#define DSTR_CSTR(ds)   (ds)->cstr
#define DSTR_LENGTH(ds) (ds)->len_cur
#define DSTR_ALLOC(ds)  (ds)->len_max

#define DSTR_IS_NULL(ds) (((ds) == NULL) || ((ds) == &_null_dstr_struct) \
  || ((ds)->cstr == NULL) || ((ds)->cstr == _null_cstr) \
  || ((ds)->len_max > DSTR_LEN_MAX) || ((ds)->len_cur > (ds)->len_max))

#define DSTR_IS_CLIPPED(ds) ((ds)->len_cur == DSTR_LEN_MAX)

/****************************************************************
*  Dynamic string structure
*
*  A valid dstring verifies:
*    dstr != NULL_DSTR
*    cstr != NULL
*    len_max <= DSTR_LEN_MAX
*    len_cur <= len_max
*    (which also implies len_cur <= DSTR_LEN_MAX)
*
*  Allocation errors for the main structure set the dstr pointer to NULL_DSTR.
*  Allocation errors for the C string member set: cstr to NULL_CSTR, len_cur to 0, len_max to DSTR_LEN_ERR.
*  Overflow errors clip the C string member at DSTR_LEN_MAX.
*  A dstring with len_cur == DSTR_LEN_MAX is assumed to be clipped.
*  
*  NULL_DSTR is a static variable, with NULL_DSTR->cstr = ""
*  to ensure standard string functions do not crash
*/
struct _dstring_struct
{
	char *cstr;
  t_dstr_int len_cur;
  t_dstr_int len_max;
};

/****************************************************************
*  Function declarations
*/
t_dstr dstr_new        ();
t_dstr dstr_new_n      (t_dstr_int len);
t_dstr dstr_new_cstr   (const char *cstr);
t_dstr dstr_new_dstr   (const t_dstr dstr);
t_dstr dstr_new_bin    (const char *bin, t_dstr_int len);
t_dstr dstr_new_int    (__int64 i);
t_dstr dstr_new_printf (const char *format, ...);

void   dstr_free       (t_dstr *dstr);

t_dstr dstr_cpy_cstr   (t_dstr dest, const char *src);
t_dstr dstr_cpy_dstr   (t_dstr dest, const t_dstr src);
t_dstr dstr_rcpy_dstr  (t_dstr dest, const t_dstr src, t_dstr_int beg, t_dstr_int len);
t_dstr dstr_cpy_bin    (t_dstr dest, const char *src, t_dstr_int len);
t_dstr dstr_cpy_int    (t_dstr dest, __int64 i);
t_dstr dstr_cpy_printf (t_dstr dest, const char *format, ...);

t_dstr dstr_cat_cstr   (t_dstr dest, const char *src);
t_dstr dstr_cat_dstr   (t_dstr dest, const t_dstr src);
t_dstr dstr_cat_bin    (t_dstr dest, const char *src, t_dstr_int len);
t_dstr dstr_cat_int    (t_dstr dest, __int64 i);
t_dstr dstr_cat_printf (t_dstr dest, const char *format, ...);

t_dstr dstr_fit    (t_dstr dstr);
t_dstr dstr_resize (t_dstr dstr, t_dstr_int len);
t_dstr dstr_empty  (t_dstr dstr);
t_dstr dstr_update (t_dstr dstr);

#endif
