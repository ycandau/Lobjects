#include "Lobjects.h"

/****************************************************************
*  Extern variables for frequently used symbols
*/
t_symbol *sym_int;
t_symbol *sym_float;
t_symbol *sym_list;
t_symbol *sym_mess;
t_symbol *sym_empty;
t_symbol *sym_null;

/****************************************************************
*  Initialize the extern variables defined for frequently used symbols
*/
void sym_init()
{
  sym_int    = gensym("int");
  sym_float  = gensym("float");
  sym_list   = gensym("list");
  sym_mess   = gensym("mess");
  sym_empty  = gensym("empty");
  sym_null   = gensym("null");
}

/****************************************************************
*  Initialize a message structure
*
*  To be called first and once.
*  Use mess_realloc() after to allocate.
*  Use mess_clear() to free the allocated memory.
*/
void mess_init(t_mess mess)
{
  mess->len_cur = 0;
  mess->len_max = 0;
  mess->sym     = sym_null;
  mess->list    = NULL;
  mess->offset  = 0;
}

/****************************************************************
*  Allocate the members in a message structure
*
*  Can be used repeatedly.
*  It is assumed that mess_init() was called first.
*  Use mess_clear() to free the allocated memory.
*/
void mess_realloc(t_mess mess, t_mess_int len_max, void *x)
{
  // Free the list if already allocated
  if (mess->list) { sysmem_freeptr(mess->list); mess->list = NULL; }

  // Allocate the list
  mess->list = (t_atom *)sysmem_newptr(len_max * sizeof(t_atom));
  //DEBUG_ALLOC(mess->list);    @NB
  
  // Test the allocation
  if (mess->list) {
    mess->len_max = len_max;
    mess_set_empty(mess);
  }
  else {
    mess_init(mess);
    ERR("Allocation error. Try resetting maxlen.");
  }
}

/****************************************************************
*  Free the members in a message structure
*/
void mess_clear(t_mess mess)
{
  if (mess->list) { sysmem_freeptr(mess->list); }
  mess_init(mess);
}

/****************************************************************
*  Set a message structure to empty
*/
void mess_set_empty(t_mess mess)
{
  mess->len_cur = 0;
  for (t_int32 i = 0; i < mess->len_max; i++) { atom_setlong(mess->list + i, 0); }

  mess->offset = 0;
  mess->sym = sym_empty;
}

/****************************************************************
*  Helper function to set a message structure
*
*  For messages with a leading symbol (not int, float, or lists)
*  the leading symbol is stored in the first atom, and the array
*  of atoms shifted accordingly.
*
*  NULL messages are asserted upon entry.
*/
void mess_set(t_mess mess, t_symbol *sym, t_mess_int argc, t_atom *argv, char offset,
  void *x, char warn)
{
  ASSERT_MESS(mess);

  // Store the leading symbol in the first atom
  // It will be overwritten if offset is 0
  atom_setsym(mess->list, sym);
  
  // Truncate the message if necessary
  if (argc > mess->len_max - offset) {
    WARN(warn, "Message truncated from length %i to %i.", argc + offset, mess->len_max);
    argc = mess->len_max - offset;
  }

  // Set the message members
  mess->len_cur = argc + offset;
  mess->sym     = sym;
  mess->offset  = offset;
  ATOMS_COPY(mess->list + offset, argv, argc);
}

/****************************************************************
*  Fill a message structure with an int value
*/
void mess_fill_int(t_mess mess, t_atom_long val, t_mess_int len)
{
  mess->len_cur = len;
  for (t_int32 i = 0; i < mess->len_max; i++) { atom_setlong(mess->list + i, val); }

  mess->offset = 0;
  switch (len) {
  case 0:  mess->sym = sym_empty; break;
  case 1:  mess->sym = sym_int; break;
  default: mess->sym = sym_list; break;
  }
}

/****************************************************************
*  Fill a message structure with a float value
*/
void mess_fill_float(t_mess mess, t_atom_float val, t_mess_int len)
{
  mess->len_cur = len;
  for (t_int32 i = 0; i < mess->len_max; i++) { atom_setfloat(mess->list + i, val); }

  mess->offset = 0;
  switch (len) {
  case 0:  mess->sym = sym_empty; break;
  case 1:  mess->sym = sym_float; break;
  default: mess->sym = sym_list; break;
  }
}

/****************************************************************
*  Fill a message structure with a symbol value
*/
void mess_fill_sym(t_mess mess, t_symbol *sym, t_mess_int len)
{
  mess->len_cur = len;
  for (t_int32 i = 0; i < mess->len_max; i++) { atom_setsym(mess->list + i, sym); }

  switch (len) {
  case 0:  mess->offset = 0; mess->sym = sym_empty; break;
  default: mess->offset = 1; mess->sym = sym; break;
  }
}

/****************************************************************
*  Fill a message structure with a value defined by an atom
*/
void mess_fill_atom(t_mess mess, t_atom *atom, t_mess_int len)
{
  mess->len_cur = len;
  for (t_int32 i = 0; i < mess->len_max; i++) { mess->list[i] = *atom; }
  mess_set_type(mess);
}

/****************************************************************
*  Set a message structure's type depending on its contents
*
*  The offset and symbol members are set.
*/
void mess_set_type(t_mess mess)
{
  if (MESS_IS_NULL(mess)) { mess->offset = 0; mess->sym = sym_null; return; }

  long type = atom_gettype(mess->list);

  switch (mess->len_cur) {
  case 0:         mess->offset = 0; mess->sym = sym_empty; return;
  case 1:
    switch (type) {
    case A_LONG:  mess->offset = 0; mess->sym = sym_int; return;
    case A_FLOAT: mess->offset = 0; mess->sym = sym_float; return;
    default:      mess->offset = 1; mess->sym = atom_getsym(mess->list); return;
    }
  default:
    switch (type) {
    case A_LONG:
    case A_FLOAT: mess->offset = 0; mess->sym = sym_list; return;
    default:      mess->offset = 1; mess->sym = atom_getsym(mess->list); return;
    }
  }
}

/****************************************************************
*  Post the content of a message structure in the console
*/
void mess_post(t_mess mess, const char *name, void *x)
{
  t_dstr dstr = dstr_new_printf("%s (%s - %i / %i) : ", name,
    (mess->offset) ? sym_mess->s_name : mess->sym->s_name, mess->len_cur, mess->len_max);

  if (MESS_IS_NULL(mess)) { dstr_cat_cstr(dstr, " <NULL>"); }
  else if (mess->len_cur == 0) { dstr_cat_cstr(dstr, " <empty>"); }
  
  else {
    for (t_mess_int i = 0; i < mess->len_cur; i++) {
      dstr_cat_bin(dstr, " ", 1);
      switch (atom_gettype(mess->list + i)) {
      case A_LONG:  dstr_cat_printf(dstr, "%i", atom_getlong(mess->list + i)); break;
      case A_FLOAT: dstr_cat_printf(dstr, "%f", atom_getfloat(mess->list + i)); break;
      case A_SYM:   dstr_cat_cstr(dstr, atom_getsym(mess->list + i)->s_name); break;
      }
    }
  }
  
  POST(dstr->cstr);
  dstr_free(&dstr);
}
