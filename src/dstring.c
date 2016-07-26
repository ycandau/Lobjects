#include "dstring.h"

/****************************************************************
*  Additions for use with the Max SDK
*/
#include "ext.h"

#define MALLOC(size) sysmem_newptr((long)(size))
#define FREE(ptr)    sysmem_freeptr((ptr))
#define MEMCPY(dest, src, len) do { memcpy((dest), (src), (len)); (dest)[(len)] = '\0'; } while (0)

/****************************************************************
*  Unexposed preprocessor macros
*/
#define NULL_DSTR &_null_dstr_struct
#define NULL_CSTR _null_cstr

#define DSTR_SET_TO_NULL(ds) do { (ds)->cstr = NULL_CSTR; (ds)->len_cur = 0; (ds)->len_max = DSTR_LEN_ERR; } while (0)

#define DSTR_ASSERT(ds)             do { if (DSTR_IS_NULL(ds)) { return (ds);  } } while (0)
#define DSTR_ASSERT_RET(ds, ret)    do { if (DSTR_IS_NULL(ds)) { return (ret); } } while (0)
#define DSTR_ASSERT_BINA(dest, src) do { if (DSTR_IS_NULL(src)) { FREE((dest)->cstr); DSTR_SET_TO_NULL(dest); return (dest); } } while (0)

/****************************************************************
*  Extern variables definition
*/
char _null_cstr[] = "<NULL>";
t_dstr_struct  _null_dstr_struct = { _null_cstr, 0, DSTR_LEN_ERR };

/****************************************************************
*  Function declarations withheld from the header file
*/
t_dstr _dstr_cstr_alloc(t_dstr dstr, const char *src, t_dstr_int len_cur, t_dstr_int len_max, t_dstr_int len_cpy);
t_dstr_int _dstr_cstr_adjust(t_dstr dest, t_dstr_int insert_pos, t_dstr_int len_cpy);
t_dstr _dstr_new (const char* src, t_dstr_int len);
t_dstr _dstr_cpycat (t_dstr dest, const char *src, t_dstr_int insert_pos, t_dstr_int len_cpy);
int _dstr_itoa (char *str, __int64 i);

/****************************************************************
*  Helper function to allocate a new string member and copy into it.
*
*  @param dstr The dstring within which to allocate.
*  @param src A pointer to a source to copy from.
*  @param len_cur To set the current length of the dstring.
*  @param len_max To set the maximum length of the dstring.
*  @param len_cpy The copy length.
*
*  @return The dstring, set to NULL values if there is an allocation error.
*/
t_dstr _dstr_cstr_alloc(t_dstr dstr, const char *src, t_dstr_int len_cur, t_dstr_int len_max, t_dstr_int len_cpy)
{
  dstr->len_cur = len_cur;
  dstr->len_max = len_max;

  // Allocate a new string pointer
  dstr->cstr = (char *)MALLOC(sizeof(char) * (len_max + 1));
  
  // Test the allocation and copy the string
  if (!dstr->cstr) { DSTR_SET_TO_NULL(dstr); }
  else { MEMCPY(dstr->cstr, src, len_cpy); }

  return dstr;
}

/****************************************************************
*  Helper function to create a dstring.
*
*  @param len The length to allocate.
*  @param src NULL or a pointer to a source to copy.
*
*  @return The new dstring, or NULL_DSTR if there is an allocation error.
*/
t_dstr _dstr_new(const char* src, t_dstr_int len)
{
  t_dstr dstr = (t_dstr)MALLOC(sizeof(t_dstr_struct));
  if (!dstr) { return NULL_DSTR; }

  len = min(len, DSTR_LEN_MAX);
  t_dstr_int len_cur = src ? len : 0;

  return _dstr_cstr_alloc(dstr, src, len_cur, len, len_cur);
}

/****************************************************************
*  Constructor to create an empty dstring.
* 
*  @return The new dstring, or NULL_DSTR if there is an allocation error.
*/
t_dstr dstr_new()
{
  return _dstr_new(NULL, 8);  // Set size to 8 as it will likely be copied into
}

/****************************************************************
*  Constructor to create an empty dstring of length up to N.
*
*  @param len The length to allocate.
*
*  @return The new dstring, or NULL_DSTR if there is an allocation error.
*/
t_dstr dstr_new_n(t_dstr_int len)
{
  return _dstr_new(NULL, len);
}

/****************************************************************
*  Constructor to create a dstring from a C string.
*
*  @param cstr A C string with which to initialize the dstring.
*
*  @return The new dstring, or NULL_DSTR if there is an allocation error.
*/
t_dstr dstr_new_cstr(const char *cstr)
{
  return _dstr_new(cstr, cstr ? (t_dstr_int)strlen(cstr) : 0);
}

/****************************************************************
*  Constructor to create a dstring from a dstring.
*
*  @param dstr A dstring with which to initialize the dstring.
*
*  @return The new dstring, or NULL_DSTR if there is an allocation error.
*/
t_dstr dstr_new_dstr(const t_dstr src)
{
  if (DSTR_IS_NULL(src)) {
    t_dstr dstr = dstr_new_n(0);
    DSTR_ASSERT(dstr);
    FREE(dstr->cstr);
    DSTR_SET_TO_NULL(dstr);
    return dstr;
  }

  return _dstr_new(src->cstr, src->len_cur);
}

/****************************************************************
*  Constructor to create a dstring from a binary string.
*
*  @param dstr A binary string with which to initialize the dstring.
*  @param len The length of the binary string.
*
*  @return The new dstring, or NULL_DSTR if there is an allocation error.
*/
t_dstr dstr_new_bin(const char *bin, t_dstr_int len)
{
  return _dstr_new(bin, len);
}

/****************************************************************
*  Constructor to create a dstring from an int value.
*
*  @param i The int value to convert.
*
*  @return The new dstring, or NULL_DSTR if there is an allocation error.
*/
t_dstr dstr_new_int(__int64 i)
{
  char cstr[DSTR_LEN_NTOA];
  t_dstr_int len = _dstr_itoa(cstr, i);

  return _dstr_new(cstr, len);
}

/****************************************************************
*  Constructor to create a dstring from a printf style string and arguments.
*
*  @param format The printf style formatting string.
*  @param ... The variadic arguments for printf.
*
*  @return The new dstring, or NULL_DSTR if there is an allocation error.
*/
t_dstr dstr_new_printf(const char *format, ...)
{
  char cstr[DSTR_LEN_PRINTF];
  t_dstr dstr;

  va_list ap;
  va_start(ap, format);
  t_dstr_int len_cpy = vsnprintf(cstr, DSTR_LEN_PRINTF, format, ap);

  // If the formatting string is invalid return an empty dstring
  if (len_cpy == -1) { dstr = _dstr_new(NULL, 0); }

  // If the buffer is long enough for what we need to copy
  else if (len_cpy < DSTR_LEN_PRINTF) { dstr = _dstr_new(cstr, len_cpy); }

  // Otherwise run printf again, directly into the dstring
  else {
    dstr = _dstr_new(NULL, len_cpy);
    if (!DSTR_IS_NULL(dstr)) {
      vsnprintf(dstr->cstr, dstr->len_max + 1, format, ap);
      dstr->len_cur = min(len_cpy, dstr->len_max);
    }
  }

  va_end(ap);
  return dstr;
}

/****************************************************************
*  Constructor to create a dstring from a double value.
*
*  @param f The double value to convert.
*  @param prec The number of digits after the decimal point.
*  @return The dstring.
*/
/*t_dstr dstr_new_float(double f, int prec)
{
  /*char cstr[2 * DSTR_LEN_NTOA];
  t_dstr_int len = dstr_ftoa(cstr, f, prec);

  return _dstr_new(len, cstr);

  return NULL_DSTR;
}*/

/****************************************************************
*  Destructor to free a dstring.
*
*  The dstring is set to NULL_DSTR.
*
*  @param dstr A pointer to the dstring to free.
*/
void dstr_free(t_dstr *p_dstr)
{
  if (p_dstr == NULL) { return; }

  if (*p_dstr == NULL) { *p_dstr = NULL_DSTR; return; }

  if (*p_dstr == NULL_DSTR) { return; }
  
  if (((*p_dstr)->cstr) && ((*p_dstr)->cstr != NULL_CSTR)) { FREE((*p_dstr)->cstr); }
	FREE(*p_dstr);
	*p_dstr = NULL_DSTR;
}

/****************************************************************
*  Helper function to adjust the size of a dstring if necessary.
*
*  @param dstr The dstring to copy or concatenate into.
*  @param insert_pos The position at which to insert or concatenate.
*  @param len_cat The copying size that the dstring should accomodate.
*
*  @return The dstring.
*/
t_dstr_int _dstr_cstr_adjust(t_dstr dest, t_dstr_int insert_pos, t_dstr_int len_cpy)
{
  DSTR_ASSERT_RET(dest, 0);

  // Clip the length, which also eliminates potential int overflows
  len_cpy = min(len_cpy, DSTR_LEN_MAX - insert_pos);
  dest->len_cur = insert_pos + len_cpy;
  t_dstr_int power = dest->len_cur;

  // Realloc if necessary
  if (power > dest->len_max) {
   
    // For small values expand dirctly to 8
    if (power < 8) { power = 8; }

    // For large values expand to the maximum value
    else if (power > (DSTR_LEN_ERR >> 1)) { power = DSTR_LEN_MAX; }

    // Otherwise get the smallest power of two greater than the value
    else {
      power |= (power >> 1);
      power |= (power >> 2);
      power |= (power >> 4);    // 8 bit type
#if (DSTR_INT_SIZE > 8)
      power |= (power >> 8);    // 16 bit type
#if (DSTR_INT_SIZE > 16)
      power |= (power >> 16);   // 32 bit type
#if (DSTR_INT_SIZE > 32)
      power |= (power >> 32);   // 64 bit type
#endif
#endif 
#endif
      power++;
    }

    // Clip to DSTR_LEN_MAX
    power = (power < DSTR_LEN_MAX) ? power : DSTR_LEN_MAX;

    // Realloc the string and test
    char *cstr_old = dest->cstr;
    _dstr_cstr_alloc(dest, cstr_old, dest->len_cur, power, insert_pos);
    FREE(cstr_old);
  }

  return len_cpy;
}

/****************************************************************
*  Helper function to copy and concatenate into a dstring.
*
*  @param dstr The dstring to copy or concatenate into.
*  @param src The source from which to copy.
*  @param insert_pos The position at which to insert or concatenate.
*  @param len_cpy The copying size that the dstring should accomodate.
*
*  @return The dstring.
*/
t_dstr _dstr_cpycat(t_dstr dest, const char *src, t_dstr_int insert_pos, t_dstr_int len_cpy)
{
  // If the source string pointer is NULL, or the copy length is 0, do nothing
  if (!src) { return dest; }

  // Adjust the dstring and test
  len_cpy = _dstr_cstr_adjust(dest, insert_pos, len_cpy);
  DSTR_ASSERT(dest);

  // Copy or concatenate
  MEMCPY(dest->cstr + insert_pos, src, len_cpy);

  return dest;
}

/****************************************************************
*  Copy a C string into a dstring.
*  
*  @param dest The dstring to copy into.
*  @param src The source string to copy from.
*
*  @return The dstring.
*/
t_dstr dstr_cpy_cstr(t_dstr dest, const char *src)
{
  return _dstr_cpycat(dest, src, 0, (t_dstr_int)strlen(src));
}

/****************************************************************
*  Copy a dstring into a dstring.
*
*  @param dest The dstring to copy into.
*  @param src The source dstring to copy from.
*
*  @return The dstring.
*/
t_dstr dstr_cpy_dstr(t_dstr dest, const t_dstr src)
{
  // Test the source dstring, necessary to propagate errors
  DSTR_ASSERT_BINA(dest, src);

  return _dstr_cpycat(dest, src->cstr, 0, src->len_cur);
}

/****************************************************************
*  Copy a portion of a dstring into a dstring.
*
*  @param dest The dstring to copy into.
*  @param src The source dstring to copy from.
*
*  @return The dstring.
*/
t_dstr dstr_rcpy_dstr(t_dstr dest, const t_dstr src, t_dstr_int beg, t_dstr_int len)
{
  // Test the source dstring, necessary to propagate errors
  DSTR_ASSERT_BINA(dest, src);

  beg = min(beg, src->len_cur);

  return _dstr_cpycat(dest, src->cstr + beg, 0, min(len, src->len_cur - beg));
}

/****************************************************************
*  Copy a binary string into a dstring.
*
*  @param dest The dstring to copy into.
*  @param src The source binary string to copy from.
*
*  @return The dstring.
*/
t_dstr dstr_cpy_bin(t_dstr dest, const char *src, t_dstr_int len)
{
  return _dstr_cpycat(dest, src, 0, len);
}

/****************************************************************
*  Copy a string from an int value into a dstring.
*
*  @param dest The dstring to copy into.
*  @param i The int value to create a string and copy from.
*
*  @return The dstring.
*/
t_dstr dstr_cpy_int(t_dstr dest, __int64 i)
{
  char cstr[DSTR_LEN_NTOA];
  t_dstr_int len = _dstr_itoa(cstr, i);

  return _dstr_cpycat(dest, cstr, 0, len);
}

/****************************************************************
*  Copy a printf style generated string into a dstring.
*
*  @param dest The dstring to copy into.
*  @param format The printf style formatting string.
*  @param ... The variadic arguments for printf.
*
*  @return The dstring.
*/
t_dstr dstr_cpy_printf(t_dstr dest, const char *format, ...)
{
  DSTR_ASSERT(dest);

  char cstr[DSTR_LEN_PRINTF];

  va_list ap;
  va_start(ap, format);
  t_dstr_int len_cpy = vsnprintf(cstr, DSTR_LEN_PRINTF, format, ap);

  // If the formatting string is invalid do nothing
  if (len_cpy == -1) { ; }

  else {
    len_cpy = _dstr_cstr_adjust(dest, 0, len_cpy);    // len_cpy is clipped if the string goes over DSTR_MAX_LEN

    if (!DSTR_IS_NULL(dest)) {

      // If the buffer is long enough for what we need to copy
      if (len_cpy < DSTR_LEN_PRINTF) { MEMCPY(dest->cstr, cstr, len_cpy); }

      // Otherwise run printf again, directly into the dstring
      else { vsnprintf(dest->cstr, len_cpy + 1, format, ap); }
    }
  }

  va_end(ap);
  return dest;
}

/****************************************************************
*  Concatenate a C string into a dstring.
*
*  @param dest The dstring to concatenate into.
*  @param src The source string to concatenate from.
*
*  @return The dstring.
*/
t_dstr dstr_cat_cstr(t_dstr dest, const char *src)
{
  return _dstr_cpycat(dest, src, dest->len_cur, (t_dstr_int)strlen(src));
}

/****************************************************************
*  Concatenate a dstring into a dstring.
*
*  @param dest The dstring to concatenate into.
*  @param src The source dstring to concatenate from.
*
*  @return The dstring.
*/
t_dstr dstr_cat_dstr(t_dstr dest, const t_dstr src)
{
  // Test the source dstring, necessary to propagate errors
  DSTR_ASSERT_BINA(dest, src);

  return _dstr_cpycat(dest, src->cstr, dest->len_cur, src->len_cur);
}

/****************************************************************
*  Concatenate a binary string into a dstring.
*
*  @param dest The dstring to concatenate into.
*  @param src The source binary string to concatenate from.
*
*  @return The dstring.
*/
t_dstr dstr_cat_bin(t_dstr dest, const char *src, t_dstr_int len)
{
  return _dstr_cpycat(dest, src, dest->len_cur, len);
}

/****************************************************************
*  Concatenate a string from an int value into a dstring.
*
*  @param dest The dstring to concatenate into.
*  @param i The int value to create a string and concatenate from.
*
*  @return The dstring.
*/
t_dstr dstr_cat_int(t_dstr dest, __int64 i)
{
  char cstr[DSTR_LEN_NTOA];
  t_dstr_int len = _dstr_itoa(cstr, i);

  return _dstr_cpycat(dest, cstr, dest->len_cur, len);
}

/****************************************************************
*  Concatenate a printf style generated string into a dstring.
*
*  @param dest The dstring to concatenate into.
*  @param format The printf style formatting string.
*  @param ... The variadic arguments for printf.
*
*  @return The dstring.
*/
t_dstr dstr_cat_printf(t_dstr dest, const char *format, ...)
{
  DSTR_ASSERT(dest);

  char cstr[DSTR_LEN_PRINTF];

  va_list ap;
  va_start(ap, format);
  t_dstr_int len_cpy = vsnprintf(cstr, DSTR_LEN_PRINTF, format, ap);
  t_dstr_int len_cur = dest->len_cur;

  // If the formatting string is invalid do nothing
  if (len_cpy == -1) { ; }

  else {
    len_cpy = _dstr_cstr_adjust(dest, len_cur, len_cpy);    // len_cpy is clipped if the string goes over DSTR_MAX_LEN

    if (!DSTR_IS_NULL(dest)) {

      // If the buffer is long enough for what we need to copy
      if (len_cpy < DSTR_LEN_PRINTF) { MEMCPY(dest->cstr + len_cur, cstr, len_cpy); }

      // Otherwise run printf again, directly into the dstring
      else { vsnprintf(dest->cstr + len_cur, len_cpy + 1, format, ap); }
    }
  }

  va_end(ap);
  return dest;
}

/****************************************************************
*  Resize the dstring to fit the C string.
*
*  @param dstr The dstring to resize.
*
*  @return The dstring.
*/
t_dstr dstr_fit(t_dstr dstr)
{
  DSTR_ASSERT(dstr);
  char *cstr_old = dstr->cstr;
  _dstr_cstr_alloc(dstr, cstr_old, dstr->len_cur, dstr->len_cur, dstr->len_cur);
  FREE(cstr_old);

  return dstr;
}

/****************************************************************
*  Resize the dstring to a set length, clipping the C string of necessary.
*
*  @param dstr The dstring to resize.
*  @param len The length to resize to.
*
*  @return The dstring.
*/
t_dstr dstr_resize(t_dstr dstr, t_dstr_int len)
{
  DSTR_ASSERT(dstr);
  len = min(len, DSTR_LEN_MAX);
  t_dstr_int len_cur = min(len, dstr->len_cur);
  char *cstr_old = dstr->cstr;
  _dstr_cstr_alloc(dstr, cstr_old, len_cur, len, len_cur);
  FREE(cstr_old);

  return dstr;
}

/****************************************************************
*  Set a dstring to empty, without resizing.
*
*  @param dstr The dstring to set to empty.
*
*  @return The dstring.
*/
t_dstr dstr_empty(t_dstr dstr)
{
  DSTR_ASSERT(dstr);
  dstr->cstr[0] = '\0';
  dstr->len_cur = 0;

  return dstr;
}

/****************************************************************
*  Update the current length of a dstring, in case its C string was modified.
*
*  @param dstr The dstring to update.
*
*  @return The dstring.
*/
t_dstr dstr_update(t_dstr dstr)
{
  DSTR_ASSERT(dstr);
  char *pc = dstr->cstr;
  t_dstr_int len = 0;

  while ((*pc != '\0') && (len != dstr->len_max)) { pc++; len++; }
  dstr->len_cur = len;
  
  return dstr;
}

/****************************************************************
*  Convert an int value into a C string.
*
*  @param str A pointer to the C string.
*  @param i The int value to convert.
*
*  @return The length of the string.
*/
int _dstr_itoa(char *str, __int64 i)
{
  char *pc = str;
  unsigned __int64 ui = (i > 0) ? i : -i;

  // Copy each digit from the lowest
  do {
    *pc++ = '0' + (ui % 10);
    ui /= 10;
  } while (ui);

  // Add a minus sign if necessary, and the terminal character
  if (i < 0) { *pc++ = '-'; }
  *pc = '\0';

  // Calculate the length
  ui = pc-- - str;

  // Reverse the string
  char tmp;
  while (str < pc) {
    tmp = *str;
    *str++ = *pc;
    *pc-- = tmp;
  }

  return (int)ui;
}
