/**
*  @file
*  Lmin - a Max object to generate the minimum of two lists
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
typedef struct _lmin
{
  t_object obj;

  // Inlets, proxies and outlets
  void *inl_proxy;
  long  inl_proxy_ind;
  void *outl_list;

  // Input and output messages
  t_mess_struct i_list_1[1];
  t_mess_struct i_list_2[1];
  t_mess_struct o_list[1];

  // Attributes
  t_mess_int maxlen;     // maximum list length
  char       warnings;   // report warnings or not

} t_lmin;

/****************************************************************
*  Global class pointer
*/
static t_class *lmin_class = NULL;

/****************************************************************
*  Function declarations
*/
void *lmin_new      (t_symbol *sym, long argc, t_atom *argv);
void  lmin_free     (t_lmin *x);
void  lmin_assist   (t_lmin *x, void *b, long msg, long arg, char *dst);

void  lmin_bang     (t_lmin *x);
void  lmin_int      (t_lmin *x, t_atom_long n);
void  lmin_float    (t_lmin *x, double f);
void  lmin_list     (t_lmin *x, t_symbol *sym, long argc, t_atom *argv);
void  lmin_anything (t_lmin *x, t_symbol *sym, long argc, t_atom *argv);
void  lmin_clear    (t_lmin *x);
void  lmin_post     (t_lmin *x);

void  lmin_action   (t_lmin *x);
void  lmin_output   (t_lmin *x);

t_max_err lmin_maxlen_set (t_lmin *x, void *attr, long argc, t_atom *argv);

/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  // Initialize frequently used symbols
  sym_init();

  t_class *c;

  c = class_new("Lmin",
    (method)lmin_new,
    (method)lmin_free,
    (long)sizeof(t_lmin),
    NULL, A_GIMME, 0);

  class_addmethod(c, (method)lmin_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)lmin_bang,     "bang",               0);
  class_addmethod(c, (method)lmin_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)lmin_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)lmin_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)lmin_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)stdinletinfo,  "inletinfo", A_CANT,  0);
  class_addmethod(c, (method)lmin_clear,    "clear",              0);
  class_addmethod(c, (method)lmin_post,     "post",               0);

  // Define the class attributes
  CLASS_ATTR_INT32    (c, "maxlen", 0, t_lmin, maxlen);
  CLASS_ATTR_ORDER    (c, "maxlen", 0, "1");                    // order
  CLASS_ATTR_LABEL    (c, "maxlen", 0, "maximum list length");  // label
  CLASS_ATTR_SAVE     (c, "maxlen", 0);                         // save with patcher
  CLASS_ATTR_SELFSAVE (c, "maxlen", 0);                         // display as saved
  CLASS_ATTR_ACCESSORS(c, "maxlen", NULL, lmin_maxlen_set);

  CLASS_ATTR_CHAR     (c, "warnings", 0, t_lmin, warnings);
  CLASS_ATTR_ORDER    (c, "warnings", 0, "2");
  CLASS_ATTR_STYLE    (c, "warnings", 0, "onoff");
  CLASS_ATTR_LABEL    (c, "warnings", 0, "report warnings");
  CLASS_ATTR_FILTER_CLIP(c, "warnings", 0, 1);
  CLASS_ATTR_SAVE     (c, "warnings", 0);
  CLASS_ATTR_SELFSAVE (c, "warnings", 0);

  class_register(CLASS_BOX, c);
  lmin_class = c;
}

/****************************************************************
*  Constructor
*/
void *lmin_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_lmin *x = NULL;

  // Allocate the object and test
  x = (t_lmin *)object_alloc(lmin_class);
  if (!x) { error("Lmin:  Object allocation failed."); return NULL; }

  TRACE("lmin_new");

  // Set inlets, outlets and proxies
  x->inl_proxy_ind = 0;
  x->inl_proxy = proxy_new((t_object *)x, 1L, &x->inl_proxy_ind);
  x->outl_list = outlet_new((t_object *)x, NULL);

  // Initialize the attributes
  x->warnings = 1;
  x->maxlen = 0;

  // Initialize the message structures
  mess_init(x->i_list_1);
  mess_init(x->i_list_2);
  mess_init(x->o_list);

  // Process the attribute arguments
  attr_args_process(x, (short)argc, argv);

  // Allocate the lists if the allocation was not triggered by arguments
  if (x->maxlen == 0) {
    t_atom atom[1];
    atom_setlong(atom, MAXLEN_DEF);
    lmin_maxlen_set(x, NULL, 1, atom);
  }

  // Process the non attribute arguments to set the right input list if necessary
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
void lmin_free(t_lmin *x)
{
  TRACE("lmin_free");

  // Free the proxy
  freeobject((t_object *)x->inl_proxy);

  // Free the message structures
  mess_clear(x->i_list_1);
  mess_clear(x->i_list_2);
  mess_clear(x->o_list);
}

/****************************************************************
*  Assist
*/
void lmin_assist(t_lmin *x, void *b, long msg, long arg, char *dst)
{
  switch (msg) {
  case ASSIST_INLET:
    switch (arg) {
    case 0: sprintf(dst, "left input list (int, float, symbol, list)"); break;
    case 1: sprintf(dst, "right input list (int, float, symbol, list)"); break;
    default: break;
    }
    break;
  case ASSIST_OUTLET:
    switch (arg) {
    case 0: sprintf(dst, "minimum of the two input lists (list)"); break;
    default: break;
    }
    break;
  }
}

/****************************************************************
*  Interface functions
*/
void lmin_bang(t_lmin *x)
{
  TRACE("lmin_bang");

  lmin_output(x);
}

/****************************************************************
*  Process int inputs
*/
void lmin_int(t_lmin *x, t_atom_long n)
{
  TRACE("lmin_int");

  ASSERT_ALLOC;

  t_atom atom[1];
  atom_setlong(atom, n);

  switch (proxy_getinlet((t_object *)x)) {
  case 0:
    mess_set_int(x->i_list_1, atom, x, x->warnings);
    lmin_action(x);
    lmin_output(x);
    break;

  case 1:
    mess_set_int(x->i_list_2, atom, x, x->warnings);
    mess_zpad(x->i_list_2);   // zero pad in case the left list is longer than the right list
    lmin_action(x);
    break;
  }
}

/****************************************************************
*  Process float inputs
*/
void lmin_float(t_lmin *x, double f)
{
  TRACE("lmin_float");

  ASSERT_ALLOC;

  t_atom atom[1];
  atom_setfloat(atom, f);

  switch (proxy_getinlet((t_object *)x)) {
  case 0:
    mess_set_float(x->i_list_1, atom, x, x->warnings);
    lmin_action(x);
    lmin_output(x);
    break;

  case 1:
    mess_set_float(x->i_list_2, atom, x, x->warnings);
    mess_zpad(x->i_list_2);
    lmin_action(x);
    break;
  }
}

/****************************************************************
*  Process list inputs
*/
void lmin_list(t_lmin *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("lmin_list");

  ASSERT_ALLOC;

  switch (proxy_getinlet((t_object *)x)) {
  case 0:
    mess_set_list(x->i_list_1, argc, argv, x, x->warnings);
    lmin_action(x);
    lmin_output(x);
    break;

  case 1:
    mess_set_list(x->i_list_2, argc, argv, x, x->warnings);
    mess_zpad(x->i_list_2);
    lmin_action(x);
    break;
  }
}

/****************************************************************
*  Process symbols and non list messages
*/
void lmin_anything(t_lmin *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("lmin_anything");

  ASSERT_ALLOC;

  switch (proxy_getinlet((t_object *)x)) {
  case 0:
    mess_set_any(x->i_list_1, sym, argc, argv, x, x->warnings);
    lmin_action(x);
    lmin_output(x);
    break;

  case 1:
    mess_set_any(x->i_list_2, sym, argc, argv, x, x->warnings);
    mess_zpad(x->i_list_2);
    lmin_action(x);
    break;
  }
}

/****************************************************************
*  Clear the lists
*/
void lmin_clear(t_lmin *x)
{
  TRACE("lmin_clear");

  mess_set_empty(x->i_list_1);
  mess_set_empty(x->i_list_2);
  mess_set_empty(x->o_list);
}

/****************************************************************
*  Post information on the external
*/
void lmin_post(t_lmin *x)
{
  TRACE("lmin_post");

  POST("Max length: %i - Warnings: %i", x->maxlen, x->warnings);
  mess_post(x->i_list_1, "Left input list", x);
  mess_post(x->i_list_2, "Right input list", x);
  mess_post(x->o_list, "Output list", x);
}

/****************************************************************
*  Determine the minimum of two lists
*/
void lmin_action(t_lmin *x)
{
  TRACE("lmin_action");

  t_mess_int incr1 = 1;
  t_mess_int incr2 = 1;

  // The output list is the same length as the left input list
  // unless the left input list was a single element
  if (x->i_list_1->len_cur == 1) {
    x->o_list->len_cur = x->i_list_2->len_cur;
    incr1 = 0;    // no pointer increment
  }
  else if (x->i_list_2->len_cur == 1) {
    x->o_list->len_cur = x->i_list_1->len_cur;
    incr2 = 0;    // no pointer increment
  }
  else {
    x->o_list->len_cur = x->i_list_1->len_cur;
  }

  t_atom *in1 = x->i_list_1->list;
  t_atom *in2 = x->i_list_2->list;
  t_atom *out = x->o_list->list;
  long type1, type2;

  // Loop through the output list
  for (t_int32 i = 0; i < x->o_list->len_cur; i++) {

    type1 = atom_gettype(in1);
    type2 = atom_gettype(in2);

    // If either input is not a number, set the output to the left input
    if (((type1 != A_LONG) && (type1 != A_FLOAT)) || ((type2 != A_LONG) && (type2 != A_FLOAT))) {
      *out = *in1;
    }
    // ... if both inputs are integers, the output is an integer
    else if ((type1 == A_LONG) && (type2 == A_LONG)) {
      atom_setlong(out, MIN(atom_getlong(in1), atom_getlong(in2)));
    }
    // ... if either output is a float, the output is a float
    else {
      atom_setfloat(out, MIN(atom_getfloat(in1), atom_getfloat(in2)));
    }
    in1 += incr1;
    in2 += incr2;
    out++;
  }

  // Set the symbol type of the output list
  mess_set_type(x->o_list);
}

/****************************************************************
*  Output function
*/
__inline void lmin_output(t_lmin *x)
{
  TRACE("lmin_output");

  mess_outlet(x->o_list, x->outl_list);
}

/****************************************************************
*  Setter function for the maxlen attribute
*/
t_max_err lmin_maxlen_set(t_lmin *x, void *attr, long argc, t_atom *argv)
{
  TRACE("lmin_maxlen_set");

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
  mess_realloc(x->i_list_1, maxlen, x);
  mess_realloc(x->i_list_2, maxlen, x);
  mess_realloc(x->o_list, maxlen, x);

  // Test the allocation
  if (MESS_IS_NULL(x->i_list_1) || MESS_IS_NULL(x->i_list_2) || MESS_IS_NULL(x->o_list)) {
    mess_clear(x->i_list_1);
    mess_clear(x->i_list_2);
    mess_clear(x->o_list);
    x->maxlen = 0;
    return MAX_ERR_OUT_OF_MEM;
  }
  else {
    x->maxlen = maxlen;
    return MAX_ERR_NONE;
  }
}
