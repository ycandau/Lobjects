/**
*  @file
*  Ltoset - a Max object to create a binary list
*  
*  Original object by Peter Elsea.
*  Refactored by Yves Candau.
*
*  Main differences:
*    - The external uses the new style Max object and attributes.
*    - The arrays to store lists and messages are resizable.
*    - Non int, float or list messages can be processed (starting with a symbol).
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
typedef struct _ltoset
{
  t_object obj;

  // Inlets, proxies and outlets
  void *outl_list;

  // Input variables
  long i_value;

  // Output messages
  t_mess_struct o_list[1];

  // Attributes
  t_mess_int maxlen;     // maximum list length
  char       warnings;   // report warnings or not

} t_ltoset;

/****************************************************************
*  Global class pointer
*/
static t_class *ltoset_class = NULL;

/****************************************************************
*  Function declarations
*/
void *ltoset_new      (t_symbol *sym, long argc, t_atom *argv);
void  ltoset_free     (t_ltoset *x);
void  ltoset_assist   (t_ltoset *x, void *b, long msg, long arg, char *dst);

void  ltoset_bang     (t_ltoset *x);
void  ltoset_int      (t_ltoset *x, t_atom_long n);
void  ltoset_in1      (t_ltoset *x, t_atom_long length);
void  ltoset_in2      (t_ltoset *x, t_atom_long value);
void  ltoset_float    (t_ltoset *x, double f);
void  ltoset_list     (t_ltoset *x, t_symbol *sym, long argc, t_atom *argv);
void  ltoset_anything (t_ltoset *x, t_symbol *sym, long argc, t_atom *argv);
void  ltoset_clear    (t_ltoset *x);
void  ltoset_post     (t_ltoset *x);

void  ltoset_defaults (t_ltoset *x);
void  ltoset_action   (t_ltoset *x, long argc, t_atom *argv);
void  ltoset_output   (t_ltoset *x);

t_max_err ltoset_maxlen_set (t_ltoset *x, void *attr, long argc, t_atom *argv);

/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  // Initialize frequently used symbols
  sym_init();

  t_class *c;

  c = class_new("Ltoset",
    (method)ltoset_new,
    (method)ltoset_free,
    (long)sizeof(t_ltoset),
    NULL, A_GIMME, 0);

  class_addmethod(c, (method)ltoset_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)ltoset_bang,     "bang",               0);
  class_addmethod(c, (method)ltoset_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)ltoset_in1,      "in1",       A_LONG,  0);
  class_addmethod(c, (method)ltoset_in2,      "in2",       A_LONG,  0);
  class_addmethod(c, (method)ltoset_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)ltoset_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)ltoset_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)stdinletinfo,    "inletinfo", A_CANT,  0);
  class_addmethod(c, (method)ltoset_clear,    "clear",              0);
  class_addmethod(c, (method)ltoset_post,     "post",               0);

  // Define the class attributes
  CLASS_ATTR_INT32    (c, "maxlen", 0, t_ltoset, maxlen);
  CLASS_ATTR_ORDER    (c, "maxlen", 0, "1");                    // order
  CLASS_ATTR_LABEL    (c, "maxlen", 0, "maximum list length");  // label
  CLASS_ATTR_SAVE     (c, "maxlen", 0);                         // save with patcher
  CLASS_ATTR_SELFSAVE (c, "maxlen", 0);                         // display as saved
  CLASS_ATTR_ACCESSORS(c, "maxlen", NULL, ltoset_maxlen_set);

  CLASS_ATTR_CHAR     (c, "warnings", 0, t_ltoset, warnings);
  CLASS_ATTR_ORDER    (c, "warnings", 0, "2");
  CLASS_ATTR_STYLE    (c, "warnings", 0, "onoff");
  CLASS_ATTR_LABEL    (c, "warnings", 0, "report warnings");
  CLASS_ATTR_FILTER_CLIP(c, "warnings", 0, 1);
  CLASS_ATTR_SAVE     (c, "warnings", 0);
  CLASS_ATTR_SELFSAVE (c, "warnings", 0);

  class_register(CLASS_BOX, c);
  ltoset_class = c;
}

/****************************************************************
*  Constructor
*/
void *ltoset_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_ltoset *x = NULL;

  x = (t_ltoset *)object_alloc(ltoset_class);
  if (!x) { error("Ltoset:  Object allocation failed."); return NULL; }

  TRACE("ltoset_new");

  // Set inlets and outlets
  intin(x, 2);
  intin(x, 1);
  x->outl_list = outlet_new((t_object *)x, NULL);

  // Initialize the attributes
  x->maxlen = 0;
  x->warnings = 1;

  // Initialize the message structures
  mess_init(x->o_list);

  // Process the attribute arguments
  attr_args_process(x, (short)argc, argv);

  // Allocate the lists if the allocation was not triggered by arguments
  if (x->maxlen == 0) {
    t_atom atom[1];
    atom_setlong(atom, MAXLEN_DEF);
    ltoset_maxlen_set(x, NULL, 1, atom);
  }

  // Initialize non attribute variables
  ltoset_defaults(x);

  // First argument:  length
  if (argc >= 1) {
    if (((atom_gettype(argv) == A_LONG) || (atom_gettype(argv) == A_FLOAT))) {

      x->o_list->len_cur = CLAMP((t_mess_int)atom_getlong(argv), 1, x->maxlen);
      if ((atom_getlong(argv) < 1) || (atom_getlong(argv) > x->maxlen)) {
        WARN(x->warnings, "Arg 1:  List length:  Out of range. Clipped to [1, %i].", x->maxlen);
      }
    }
    else { ERR("Arg 1:  List length:  Invalid type (%s). Int expected.", atom_getsym(argv)->s_name); }
  }

  // Second argument:  value
  if (argc >= 2) {
    if ((atom_gettype(argv) == A_LONG) || (atom_gettype(argv) == A_FLOAT)) {
      x->i_value = (long)atom_getlong(argv + 1);
    }
    else { ERR("Arg 2:  Value:  Invalid type (%s). Int expected."); }
  }

  return x;
}

/****************************************************************
*  Destructor
*/
void ltoset_free(t_ltoset *x)
{
  TRACE("ltoset_free");

  // Free the message structures
  mess_clear(x->o_list);
}

/****************************************************************
*  Assist
*/
void ltoset_assist(t_ltoset *x, void *b, long msg, long arg, char *dst)
{
  switch (msg) {
  case ASSIST_INLET:
    switch (arg) {
    case 0: sprintf(dst, "indexes of members to set (int, float, list)"); break;
    case 1: sprintf(dst, "value to place in sets (int)"); break;
    case 2: sprintf(dst, "length of sets (int)"); break;
    default: break;
    }
    break;
  case ASSIST_OUTLET:
    switch (arg) {
    case 0: sprintf(dst, "list of %is and values (list)", x->i_value); break;
    default: break;
    }
    break;
  }
}

/****************************************************************
*  Interface functions
*/
void ltoset_bang(t_ltoset *x)
{
  TRACE("ltoset_bang");

  ltoset_output(x);
}

/****************************************************************
*  Process int inputs
*/
void ltoset_int(t_ltoset *x, t_atom_long n)
{
  TRACE("ltoset_int");

  ASSERT_ALLOC;

  t_atom atom[1];
  atom_setlong(atom, CLAMP(n, 0, x->o_list->len_cur - 1));
  ltoset_action(x, 1, atom);
  ltoset_output(x);
}

/****************************************************************
*  Set the value to use for the generated list
*/
void ltoset_in1(t_ltoset *x, t_atom_long value)
{
  TRACE("ltoset_in1");

  x->i_value = (long)value;
}

/****************************************************************
*  Set the length of the generated list
*/
void ltoset_in2(t_ltoset *x, t_atom_long length)
{
  TRACE("ltoset_in2");

  x->o_list->len_cur = CLAMP((t_mess_int)length, 1, x->maxlen);
}

/****************************************************************
*  Process float inputs
*/
void ltoset_float(t_ltoset *x, double f)
{
  TRACE("ltoset_float");

  ltoset_int(x, (t_mess_int)f);
}

/****************************************************************
*  Process list inputs
*/
void ltoset_list(t_ltoset *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("ltoset_list");

  ASSERT_ALLOC;

  ltoset_action(x, argc, argv);
  ltoset_output(x);
}

/****************************************************************
*  Process symbols and non list messages
*/
void ltoset_anything(t_ltoset *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("ltoset_anything");

  ASSERT_ALLOC;

  WARN(x->warnings, "Symbol in list. The object expects integers only.");
  ltoset_action(x, argc, argv);
  ltoset_output(x);
}

/****************************************************************
*  Clear the lists
*/
void ltoset_clear(t_ltoset *x)
{
  TRACE("ltoset_clear");

  mess_fill_int(x->o_list, 0, x->o_list->len_cur);
}

/****************************************************************
*  Post information on the external
*/
void ltoset_post(t_ltoset *x)
{
  TRACE("ltoset_post");

  POST("Max length: %i - Warnings: %i - Set value: %i - Set length: %i",
    x->maxlen, x->warnings, x->i_value, x->o_list->len_cur);
  mess_post(x->o_list, "Output list", x);
}

/****************************************************************
*  Set default values
*/
void ltoset_defaults(t_ltoset *x)
{
  x->i_value = 1;
  x->o_list->len_cur = MIN(12, x->maxlen);
}

/****************************************************************
*  The specific list action
*/
void ltoset_action(t_ltoset *x, long argc, t_atom *argv)
{
  TRACE("ltoset_action");

  // Reset the output list to 0
  mess_fill_int(x->o_list, 0, x->o_list->len_cur);

  // Set all members of the set to the value
  t_mess_int index;
  for (long i = 0; i < argc; i++) {
    if (atom_gettype(argv + i) == A_SYM) {
      WARN(x->warnings, "Symbol in list. The object expects integers only.");
    }
    else {
      index = (t_mess_int)atom_getlong(argv + i);
      if ((index >= 0) && (index < x->maxlen)) {
        atom_setlong(x->o_list->list + index, x->i_value);
      }
    }
  }

  mess_set_type(x->o_list);
}

/****************************************************************
*  Output function
*/
__inline void ltoset_output(t_ltoset *x)
{
  TRACE("ltoset_output");

  mess_outlet(x->o_list, x->outl_list);
}

/****************************************************************
*  Setter function for the maxlen attribute
*/
t_max_err ltoset_maxlen_set(t_ltoset *x, void *attr, long argc, t_atom *argv)
{
  TRACE("ltoset_maxlen_set");

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
  mess_realloc(x->o_list, maxlen, x);

  // Test the allocation
  if (MESS_IS_NULL(x->o_list)) {
    mess_clear(x->o_list);
    x->maxlen = 0;
    return MAX_ERR_OUT_OF_MEM;
  }
  else {
    x->maxlen = maxlen;
    ltoset_defaults(x);
    return MAX_ERR_NONE;
  }
}
