#ifndef YC_L_LIB_H_
#define YC_L_LIB_H_

/****************************************************************
*  Header files
*/
#include "ext.h"
#include "dstring.h"

/****************************************************************
*  Typedef and type sizes
*/
typedef struct _mess_struct t_mess_struct;
typedef struct _mess_struct * t_mess;
typedef t_int32 t_mess_int;

/****************************************************************
*  Message structure
*/
struct _mess_struct
{
  t_mess_int len_cur;
  t_mess_int len_max;
  t_symbol  *sym;
  t_atom    *list;
  char       offset;
};

/****************************************************************
*  Preprocessor macros
*/
#define MESS_IS_NULL(mess) (((mess) == NULL) || ((mess)->len_max == 0) || ((mess)->list == NULL))

#define ASSERT_MESS(mess) if (MESS_IS_NULL(mess)) { return; }

#define ASSERT_ALLOC if (!x->maxlen) { ERR("Previous allocation error. Try resetting maxlen."); return; }

#define ATOMS_COPY(dest, src, cnt) \
  sysmem_copyptr((src), (dest), (long)(sizeof(t_atom) * (cnt)))

#define TRACE(str, ...)      //object_post ((t_object *)x, "TRACE:  " str, __VA_ARGS__)
#define POST(str, ...)       object_post ((t_object *)x, (str), __VA_ARGS__)
#define WARN(warn, str, ...) if (warn) { object_warn ((t_object *)x, (str), __VA_ARGS__); }
#define ERR(str, ...)        object_error((t_object *)x, (str), __VA_ARGS__)

//#define DEBUG_ALLOC(ptr) if ((float)rand() / RAND_MAX <= 0.1) { sysmem_freeptr(ptr); (ptr) = NULL; }

#define MAXLEN_DEF 256

/****************************************************************
*  Function declarations
*/

/****************************************************************
*  Initialize the extern variables defined for frequently used symbols
*/
void sym_init        ();

/****************************************************************
*  Initialize a message structure
*
*  To be called first and once.
*  Use mess_realloc() after to allocate.
*  Use mess_clear() to free the allocated memory.
*/
void mess_init       (t_mess mess);

/****************************************************************
*  Allocate the members in a message structure
*
*  Can be used repeatedly.
*  It is assumed that mess_init() was called first.
*  Use mess_clear() to free the allocated memory.
*/
void mess_realloc    (t_mess mess, t_mess_int len_max, void *x);

/****************************************************************
*  Free the members in a message structure
*/
void mess_clear      (t_mess mess);

/****************************************************************
*  Set a message structure to empty
*/
void mess_set_empty  (t_mess mess);

/****************************************************************
*  Set a message structure to hold an int
*/
void mess_set_int    (t_mess mess, t_atom *argv, void *x, char warn);

/****************************************************************
*  Set a message structure to hold a float
*/
void mess_set_float  (t_mess mess, t_atom *argv, void *x, char warn);

/****************************************************************
*  Set a message structure to hold a list
*/
void mess_set_list   (t_mess mess, t_mess_int argc, t_atom *argv, void *x, char warn);

/****************************************************************
*  Set a message structure to hold a non-list message
*/
void mess_set_any    (t_mess mess, t_symbol *sym, t_mess_int argc, t_atom *argv, void *x, char warn);

/****************************************************************
*  Helper function to set a message structure
*
*  For messages with a leading symbol (not int, float, or lists)
*  the leading symbol is stored in the first atom, and the array
*  of atoms shifted accordingly.
*
*  NULL messages are asserted upon entry.
*/
void mess_set        (t_mess mess, t_symbol *sym, t_mess_int argc, t_atom *argv, char offset, void *x, char warn);

/****************************************************************
*  Fill a message structure with an int value
*/
void mess_fill_int   (t_mess mess, t_atom_long val, t_mess_int len);

/****************************************************************
*  Fill a message structure with a float value
*/
void mess_fill_float (t_mess mess, t_atom_float val, t_mess_int len);

/****************************************************************
*  Fill a message structure with a symbol value
*/
void mess_fill_sym   (t_mess mess, t_symbol *sym, t_mess_int len);

/****************************************************************
*  Fill a message structure with a value defined by an atom
*/
void mess_fill_atom  (t_mess mess, t_atom *atom, t_mess_int len);

/****************************************************************
*  Set a message structure's type depending on its contents
*
*  The offset and symbol members are set.
*/
void mess_set_type   (t_mess mess);

/****************************************************************
*  Pad the remainder of a message structure with zeros
*/
void mess_zpad       (t_mess mess);

/****************************************************************
*  Output the content of a message structure through an outlet
*/
void mess_outlet     (t_mess mess, void *outl);

/****************************************************************
*  Post the content of a message structure in the console
*/
void mess_post       (t_mess mess, const char *name, void *x);

/****************************************************************
*  Extern variables for frequently used symbols
*/
extern t_symbol *sym_int;
extern t_symbol *sym_float;
extern t_symbol *sym_list;
extern t_symbol *sym_mess;
extern t_symbol *sym_empty;
extern t_symbol *sym_null;

/****************************************************************
*  Inline functions definitions
*/

/****************************************************************
*  Set a message structure to hold an int
*/
__inline void mess_set_int(t_mess mess, t_atom *argv, void *x, char warn)
{
  mess_set(mess, sym_int, 1, argv, 0, x, warn);
}

/****************************************************************
*  Set a message structure to hold a float
*/
__inline void mess_set_float(t_mess mess, t_atom *argv, void *x, char warn)
{
  mess_set(mess, sym_float, 1, argv, 0, x, warn);
}

/****************************************************************
*  Set a message structure to hold a list
*/
__inline void mess_set_list(t_mess mess, t_mess_int argc, t_atom *argv, void *x, char warn)
{
  mess_set(mess, sym_list, argc, argv, 0, x, warn);
}

/****************************************************************
*  Set a message structure to hold a non-list message
*/
__inline void mess_set_any(t_mess mess, t_symbol *sym, t_mess_int argc, t_atom *argv, void *x, char warn)
{
  mess_set(mess, sym, argc, argv, 1, x, warn);
}

/****************************************************************
*  Pad the remainder of a message structure with zeros
*/
__inline void mess_zpad(t_mess mess)
{
  for (t_int32 i = mess->len_cur; i < mess->len_max; i++) { atom_setlong(mess->list + i, 0); }
}

/****************************************************************
*  Output the content of a message structure through an outlet
*/
__inline void mess_outlet(t_mess mess, void *outl)
{
  if ((mess->sym != sym_null) && (mess->sym != sym_empty)) {
    outlet_anything(outl, mess->sym, (short)(mess->len_cur - mess->offset), mess->list + mess->offset);
  }
}

#endif