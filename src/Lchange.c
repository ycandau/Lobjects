/**
*  @file
*  Lchange - a Max object to pad a list
*  
*  Original object by Peter Elsea.
*  Refactored by Yves Candau.
*
*  Main differences:
*    - The external uses the new style Max object and attributes.
*    - The arrays to store lists and messages are resizable.
*    - Non int, float or list messages can be processed (starting with a symbol).
*
*  Notes: 
*    Int and float atoms with similar values are considered equal.
*    Truncated and full length lists are considered unequal.
*/

/****************************************************************
*  Header files
*/
#include "ext.h"
#include "ext_obex.h"
#include "Lobjects.h"

/****************************************************************
*  Max object structure
*/
typedef struct _lchange
{
  t_object obj;

  // Inlets, proxies and outlets
  void *inl_proxy;
  long  inl_proxy_ind;
  void *outl_list_same;
  void *outl_list_diff;

  // Input message
  t_mess_struct i_list_2[1];

  // Attributes
  t_mess_int maxlen;     // maximum list length
  char       warnings;   // report warnings or not
  char       is_locked;  // lock or unlock the stored string

} t_lchange;

/****************************************************************
*  Global class pointer
*/
static t_class *lchange_class = NULL;

/****************************************************************
*  Function declarations
*/
void *lchange_new      (t_symbol *sym, long argc, t_atom *argv);
void  lchange_free     (t_lchange *x);
void  lchange_assist   (t_lchange *x, void *b, long msg, long arg, char *dst);

void  lchange_bang     (t_lchange *x);
void  lchange_int      (t_lchange *x, t_atom_long n);
void  lchange_float    (t_lchange *x, double f);
void  lchange_list     (t_lchange *x, t_symbol *sym, long argc, t_atom *argv);
void  lchange_anything (t_lchange *x, t_symbol *sym, long argc, t_atom *argv);
void  lchange_clear    (t_lchange *x);
void  lchange_post     (t_lchange *x);

void  lchange_action   (t_lchange *x, t_symbol *sym, long argc, t_atom *argv, char offset);
void  lchange_output   (t_lchange *x);

t_max_err lchange_maxlen_set (t_lchange *x, void *attr, long argc, t_atom *argv);

/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  // Initialize frequently used symbols
  sym_init();

  t_class *c;

  c = class_new("Lchange",
    (method)lchange_new,
    (method)lchange_free,
    (long)sizeof(t_lchange),
    NULL, A_GIMME, 0);

  class_addmethod(c, (method)lchange_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)lchange_bang,     "bang",               0);
  class_addmethod(c, (method)lchange_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)lchange_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)lchange_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)lchange_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)stdinletinfo,     "inletinfo", A_CANT,  0);
  class_addmethod(c, (method)lchange_clear,    "clear",              0);
  class_addmethod(c, (method)lchange_post,     "post",               0);

  // Define the class attributes
  CLASS_ATTR_INT32    (c, "maxlen", 0, t_lchange, maxlen);
  CLASS_ATTR_ORDER    (c, "maxlen", 0, "1");                    // order
  CLASS_ATTR_LABEL    (c, "maxlen", 0, "maximum list length");  // label
  CLASS_ATTR_SAVE     (c, "maxlen", 0);                         // save with patcher
  CLASS_ATTR_SELFSAVE (c, "maxlen", 0);                         // display as saved
  CLASS_ATTR_ACCESSORS(c, "maxlen", NULL, lchange_maxlen_set);

  CLASS_ATTR_CHAR     (c, "warnings", 0, t_lchange, warnings);
  CLASS_ATTR_ORDER    (c, "warnings", 0, "2");
  CLASS_ATTR_STYLE    (c, "warnings", 0, "onoff");
  CLASS_ATTR_LABEL    (c, "warnings", 0, "report warnings");
  CLASS_ATTR_FILTER_CLIP(c, "warnings", 0, 1);
  CLASS_ATTR_SAVE     (c, "warnings", 0);
  CLASS_ATTR_SELFSAVE (c, "warnings", 0);

  CLASS_ATTR_CHAR     (c, "lock", 0, t_lchange, is_locked);
  CLASS_ATTR_ORDER    (c, "lock", 0, "3");
  CLASS_ATTR_STYLE    (c, "lock", 0, "onoff");
  CLASS_ATTR_LABEL    (c, "lock", 0, "lock the stored list");
  CLASS_ATTR_FILTER_CLIP(c, "lock", 0, 1);
  CLASS_ATTR_SAVE     (c, "lock", 0);
  CLASS_ATTR_SELFSAVE (c, "lock", 0);
  
  // Register the class
  class_register(CLASS_BOX, c);
  lchange_class = c;
}

/****************************************************************
*  Constructor
*/
void *lchange_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_lchange *x = NULL;

  // Allocate the object and test
  x = (t_lchange *)object_alloc(lchange_class);
  if (!x) { error("Lchange:  Object allocation failed."); return NULL; }

  TRACE("lchange_new");

  // Set inlets, outlets and proxies
  x->inl_proxy_ind = 0;
  x->inl_proxy = proxy_new((t_object *)x, 1L, &x->inl_proxy_ind);
  x->outl_list_same = outlet_new((t_object *)x, NULL);
  x->outl_list_diff = outlet_new((t_object *)x, NULL);

  // Initialize the attributes
  x->maxlen = 0;
  x->warnings = 1;
  x->is_locked = 1;

  // Initialize the message structures
  mess_init(x->i_list_2);

  // Process the attribute arguments
  attr_args_process(x, (short)argc, argv);

  // Allocate the lists if the allocation was not triggered by arguments
  if (x->maxlen == 0) {
    t_atom atom[1];
    atom_setlong(atom, MAXLEN_DEF);
    lchange_maxlen_set(x, NULL, 1, atom);
  }

  // Process the non attribute arguments to set the left input list if necessary
  argc = (t_mess_int)attr_args_offset((short)argc, argv);
  if (argc) {
    mess_set_list(x->i_list_2, argc, argv, x, x->warnings);
    mess_set_type(x->i_list_2);    // determine the proper symbol (int, float, list, message)
  }
 
  return x;
}

/****************************************************************
*  Destructor
*/
void lchange_free(t_lchange *x)
{
  TRACE("lchange_free");

  // Free the proxies
  freeobject((t_object *)x->inl_proxy);

  // Free the message structures
  mess_clear(x->i_list_2);
}

/****************************************************************
*  Assist
*/
void lchange_assist(t_lchange *x, void *b, long msg, long arg, char *dst)
{
  switch (msg) {
  case ASSIST_INLET:
    switch (arg) {
    case 0: sprintf(dst, "list to test (int, float, symbol, list)"); break;
    case 1: sprintf(dst, "list to test against (int, float, symbol, list)"); break;
    default: break;
    }
    break;
  case ASSIST_OUTLET:
    switch (arg) {
    case 0: sprintf(dst, "if list is different (list)"); break;
    case 1: sprintf(dst, "if list is the same (list)"); break;
    default: break;
    }
    break;
  }
}

/****************************************************************
*  Process bang messages:  send the stored list out
*/
void lchange_bang(t_lchange *x)
{
  TRACE("lchange_bang");

  mess_outlet(x->i_list_2, x->outl_list_diff);
}

/****************************************************************
*  Process int inputs
*/
void lchange_int(t_lchange *x, t_atom_long n)
{
  TRACE("lchange_int");

  ASSERT_ALLOC;

  t_atom atom[1];
  atom_setlong(atom, n);

  switch (proxy_getinlet((t_object *)x)) {
  case 0: lchange_action(x, sym_int, 1, atom, 0); break;
  case 1: mess_set_int(x->i_list_2, atom, x, x->warnings); break;
  }
}

/****************************************************************
*  Process float inputs
*/
void lchange_float(t_lchange *x, double f)
{
  TRACE("lchange_float");

  ASSERT_ALLOC;

  t_atom atom[1];
  atom_setfloat(atom, f);

  switch (proxy_getinlet((t_object *)x)) {
  case 0: lchange_action(x, sym_float, 1, atom, 0); break;
  case 1: mess_set_float(x->i_list_2, atom, x, x->warnings); break;
  }
}

/****************************************************************
*  Process list inputs
*/
void lchange_list(t_lchange *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("lchange_list");

  ASSERT_ALLOC;

  switch (proxy_getinlet((t_object *)x)) {
  case 0: lchange_action(x, sym_list, argc, argv, 0); break;
  case 1: mess_set_list(x->i_list_2, argc, argv, x, x->warnings); break;
  }
}

/****************************************************************
*  Process other inputs
*/
void lchange_anything(t_lchange *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("lchange_anything");

  ASSERT_ALLOC;

  switch (proxy_getinlet((t_object *)x)) {
  case 0: lchange_action(x, sym, argc, argv, 1); break;
  case 1: mess_set_any(x->i_list_2, sym, argc, argv, x, x->warnings); break;
  }
}

/****************************************************************
*  Clear the lists
*/
void lchange_clear(t_lchange *x)
{
  TRACE("lchange_clear");

  mess_set_empty(x->i_list_2);
}

/****************************************************************
*  Post information
*/
void lchange_post(t_lchange *x)
{
  TRACE("lchange_post");

  POST("Max length: %i - Warnings: %i - Lock: %i",
    x->maxlen, x->warnings, x->is_locked);
  mess_post(x->i_list_2, "Stored list", x);
}

/****************************************************************
*  Helper function to test if two atoms are different
*/
__inline t_bool atoms_diff(t_atom *atom1, t_atom *atom2)
{
  return !(((atom_gettype(atom1) == A_SYM) && (atom_gettype(atom2) == A_SYM)
      && (atom_getsym(atom1) == atom_getsym(atom2)))
    || (((atom_gettype(atom1) == A_LONG) || (atom_gettype(atom1) == A_FLOAT))   // or both are numbers
      && ((atom_gettype(atom2) == A_LONG) || (atom_gettype(atom2) == A_FLOAT))
      && (atom_getfloat(atom1) == atom_getfloat(atom2))));                      // and equal
}

/****************************************************************
*  Compare the input list to the stored list
*/
void lchange_action(t_lchange *x, t_symbol *sym, long argc, t_atom *argv, char offset)
{
  TRACE("lchange_action");

  if (argc + offset > x->maxlen) {
    WARN(x->warnings, "The input message is clipped from length %i to %i.",
      argc + offset, x->maxlen);
  }

  t_bool match = true;

  // Proceed through a series of matching tests
  if ((argc + offset != x->i_list_2->len_cur)       // matching lengths
    || ((sym != x->i_list_2->sym)                   // matching leading symbols
      && (sym != sym_int) && (sym != sym_float)))   // special case for int and float with same value
    { match = false; }

  // Test the remaining elements of the list
  else {
    for (t_mess_int i = 0; i < x->i_list_2->len_cur - offset; i++) {
      if (atoms_diff(argv + i, x->i_list_2->list + offset + i)) { match = false; break; } }
  }

  // If the matching failed
  if (match == false) {
    
    // If the stored list is not locked, replace it with the input list
    if (!x->is_locked) { mess_set(x->i_list_2, sym, argc, argv, offset, x, x->warnings); }

    // Send the list out of the left inlet
    outlet_anything(x->outl_list_diff, sym, (short)argc, argv);
  }

  // Send the list out of the right inlet
  else {
    outlet_anything(x->outl_list_same, sym, (short)argc, argv);
  }
}

/****************************************************************
*  Output function
*/
__inline void lchange_output(t_lchange *x)
{
  TRACE("lchange_output");

  mess_outlet(x->i_list_2, x->outl_list_diff);
}

/****************************************************************
*  Setter function for the maxlen attribute
*/
t_max_err lchange_maxlen_set(t_lchange *x, void *attr, long argc, t_atom *argv)
{
  TRACE("lchange_maxlen_set");

  // If no arguments, do nothing
  if (!argc || !argv) { return MAX_ERR_GENERIC; }

  // Get the length and test the value
  t_mess_int maxlen = (t_mess_int)atom_getlong(argv);
  if (maxlen <= 0) {
    WARN(x->warnings, "maxlen:  Invalid value: %i - Expected: int >= 1 - Default used: %i", maxlen, MAXLEN_DEF);
    maxlen = MAXLEN_DEF;
  }
  if (maxlen == x->maxlen) { return MAX_ERR_NONE; }

  // Realloc the lists
  mess_realloc(x->i_list_2, maxlen, x);

  // Test the allocation
  if (MESS_IS_NULL(x->i_list_2)) {
    mess_clear(x->i_list_2);
    x->maxlen = 0;
    return MAX_ERR_OUT_OF_MEM;
  }
  else {
    x->maxlen = maxlen;
    return MAX_ERR_NONE;
  }
}
