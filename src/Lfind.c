/**
*  @file
*  Lfind - a Max object to find a list or value in a list
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
typedef struct _lfind
{
  t_object obj;

  // Inlets, proxies and outlets
  void *inl_proxy;
  long  inl_proxy_ind;
  void *outl_float;

  // Input messages
  t_mess_struct i_list_2[1];

  // Output variable
  double     o_float;

  // Attributes
  t_mess_int maxlen;     // maximum list length
  char       warnings;   // report warnings or not

} t_lfind;

/****************************************************************
*  Global class pointer
*/
static t_class *lfind_class = NULL;

/****************************************************************
*  Function declarations
*/
void *lfind_new      (t_symbol *sym, long argc, t_atom *argv);
void  lfind_free     (t_lfind *x);
void  lfind_assist   (t_lfind *x, void *b, long msg, long arg, char *dst);

void  lfind_bang     (t_lfind *x);
void  lfind_int      (t_lfind *x, t_atom_long n);
void  lfind_float    (t_lfind *x, double f);
void  lfind_list     (t_lfind *x, t_symbol *sym, long argc, t_atom *argv);
void  lfind_anything (t_lfind *x, t_symbol *sym, long argc, t_atom *argv);
void  lfind_clear    (t_lfind *x);
void  lfind_post     (t_lfind *x);

void  lfind_action   (t_lfind *x, long argc, t_atom *argv, double f);
void  lfind_output   (t_lfind *x);

t_max_err lfind_maxlen_set (t_lfind *x, void *attr, long argc, t_atom *argv);

/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  // Initialize frequently used symbols
  sym_init();

  t_class *c;

  c = class_new("Lfind",
    (method)lfind_new,
    (method)lfind_free,
    (long)sizeof(t_lfind),
    NULL, A_GIMME, 0);

  class_addmethod(c, (method)lfind_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)lfind_bang,     "bang",               0);
  class_addmethod(c, (method)lfind_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)lfind_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)lfind_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)lfind_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)stdinletinfo,   "inletinfo", A_CANT,  0);
  class_addmethod(c, (method)lfind_clear,    "clear",              0);
  class_addmethod(c, (method)lfind_post,     "post",               0);

  // Define the class attributes
  CLASS_ATTR_INT32    (c, "maxlen", 0, t_lfind, maxlen);
  CLASS_ATTR_ORDER    (c, "maxlen", 0, "1");                    // order
  CLASS_ATTR_LABEL    (c, "maxlen", 0, "maximum list length");  // label
  CLASS_ATTR_SAVE     (c, "maxlen", 0);                         // save with patcher
  CLASS_ATTR_SELFSAVE (c, "maxlen", 0);                         // display as saved
  CLASS_ATTR_ACCESSORS(c, "maxlen", NULL, lfind_maxlen_set);

  CLASS_ATTR_CHAR     (c, "warnings", 0, t_lfind, warnings);
  CLASS_ATTR_ORDER    (c, "warnings", 0, "2");
  CLASS_ATTR_STYLE    (c, "warnings", 0, "onoff");
  CLASS_ATTR_LABEL    (c, "warnings", 0, "report warnings");
  CLASS_ATTR_FILTER_CLIP(c, "warnings", 0, 1);
  CLASS_ATTR_SAVE     (c, "warnings", 0);
  CLASS_ATTR_SELFSAVE (c, "warnings", 0);

  class_register(CLASS_BOX, c);
  lfind_class = c;
}

/****************************************************************
*  Constructor
*/
void *lfind_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_lfind *x = NULL;

  x = (t_lfind *)object_alloc(lfind_class);
  if (!x) { error("Lfind:  Object allocation failed."); return NULL; }

  TRACE("lfind_new");

  // Set inlets, outlets and proxies
  x->inl_proxy_ind = 0;
  x->inl_proxy = proxy_new((t_object *)x, 1L, &x->inl_proxy_ind);
  x->outl_float = floatout((t_object *)x);

  // Initialize the attributes
  x->maxlen = 0;
  x->warnings = 1;

  // Initialize the message structures
  mess_init(x->i_list_2);

  // Process the attribute arguments
  attr_args_process(x, (short)argc, argv);

  // Allocate the lists if the allocation was not triggered by arguments
  if (x->maxlen == 0) {
    t_atom atom[1];
    atom_setlong(atom, MAXLEN_DEF);
    lfind_maxlen_set(x, NULL, 1, atom);
  }

  // Process the non attribute arguments to set the left input list if necessary
  argc = (t_mess_int)attr_args_offset((short)argc, argv);
  switch (argc) {

  // Zero arguments:  do nothing
  case 0: break;

  // One argument:  initialize the stored list to a constant list
  case 1: mess_fill_float(x->i_list_2, atom_getfloat(argv), 1); break;

  // More than one:  initialize the stored list using the arguments
  default:
    mess_set_list(x->i_list_2, argc, argv, x, x->warnings);
    mess_set_type(x->i_list_2);    // determine the proper symbol (int, float, list, message)
    break;
  }

  // Remaining variables
  x->o_float = 0.0;

  return x;
}

/****************************************************************
*  Destructor
*/
void lfind_free(t_lfind *x)
{
  TRACE("lfind_free");

  // Free the proxies
  freeobject((t_object *)x->inl_proxy);

  // Free the message structures
  mess_clear(x->i_list_2);
}

/****************************************************************
*  Assist
*/
void lfind_assist(t_lfind *x, void *b, long msg, long arg, char *dst)
{
  switch (msg) {
  case ASSIST_INLET:
    switch (arg) {
    case 0: sprintf(dst, "list to search or target to search for (int, float, symbol, list)"); break;
    case 1: sprintf(dst, "list to store or target to search for (int, float, symbol, list)"); break;
    default: break;
    }
    break;
  case ASSIST_OUTLET:
    switch (arg) {
    case 0: sprintf(dst, "position of target in list (float)"); break;
    default: break;
    }
    break;
  }
}

/****************************************************************
*  Interface functions
*/
void lfind_bang(t_lfind *x)
{
  TRACE("lfind_bang");

  lfind_output(x);
}

/****************************************************************
*  Process int inputs
*/
void lfind_int(t_lfind *x, t_atom_long n)
{
  TRACE("lfind_int");

  lfind_float(x, (double)n);
}

/****************************************************************
*  Process float inputs
*/
void lfind_float(t_lfind *x, double f)
{
  TRACE("lfind_float");

  ASSERT_ALLOC;

  switch (proxy_getinlet((t_object *)x)) {

  // Left inlet:  find the number in the stored list
  case 0:
    lfind_action(x, x->i_list_2->len_cur, x->i_list_2->list, f);
    break;

  // Right inlet:  store a constant list
  case 1: mess_fill_float(x->i_list_2, (t_atom_float)f, 1); break;
  }
}

/****************************************************************
*  Process list inputs
*/
void lfind_list(t_lfind *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("lfind_list");

  ASSERT_ALLOC;

  // Clip the list if it exceeds the maximum length
  argc = MIN(argc, x->maxlen);

  switch (proxy_getinlet((t_object *)x)) {

  // Left inlet:  search the first stored value in the incoming list
  case 0:
    lfind_action(x, argc, argv, atom_getfloat(x->i_list_2->list));
    break;

  // Right inlet:  store the incoming list
  case 1:
    mess_set_list(x->i_list_2, argc, argv, x, x->warnings);
    mess_zpad(x->i_list_2);
    break;
  }
}

/****************************************************************
*  Process symbols and non list messages
*/
void lfind_anything(t_lfind *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("lfind_anything");

  WARN(x->warnings, "Invalid input:  int or list expected.");
}

/****************************************************************
*  Clear the lists
*/
void lfind_clear(t_lfind *x)
{
  TRACE("lfind_clear");

  mess_set_empty(x->i_list_2);
  x->i_list_2->len_cur = 1;
}

/****************************************************************
*  Post information on the external
*/
void lfind_post(t_lfind *x)
{
  TRACE("lfind_post");

  POST("Max length: %i - Warnings: %i - Position found: %f",
    x->maxlen, x->warnings, x->o_float);
  mess_post(x->i_list_2, "Stored input list", x);
}

/****************************************************************
*  The specific list action
*/
void lfind_action(t_lfind *x, long argc, t_atom *argv, double f)
{
  TRACE("lfind_action");

  t_mess_int incr1 = 1;
  t_mess_int incr2 = 1;

  // Look for exact matches first
  for (t_mess_int i = 0; i < argc; i++) {
    if (atom_getfloat(argv + i) == f) {
      x->o_float = i;
      lfind_output(x);
      return;
    }
  }

  double argf, less_than_val;
  t_mess_int less_than_ind, more_than_ind;

  // Loop forward to find the highest value still less than f
  if ((argf = atom_getfloat(argv)) < f) {

    less_than_val = argf;
    less_than_ind = 0;
    for (t_mess_int i = 1; (i != argc) && ((argf = atom_getfloat(argv + i)) < f); i++) {

      if (argf >= less_than_val) {
        less_than_val = argf;
        less_than_ind = i;
      }
    }
  }
  
  // ... otherwise try looping backwards from the end
  else if ((argf = atom_getfloat(argv + argc - 1)) < f) {

    less_than_val = argf;
    less_than_ind = argc - 1;
    for (t_mess_int i = argc - 2; (i != -1) && ((argf = atom_getfloat(argv + i)) < f); i--) {

      if (argf >= less_than_val) {
        less_than_val = argf;
        less_than_ind = i;
      }
    }
  }

  // ... if none return
  else { return; }

  // Loop forward to find the lowest value still more than f
  more_than_ind = -1;
  for (t_mess_int i = less_than_ind; i != argc; i++) {
    if ((argf = atom_getfloat(argv + i)) > f) {
      more_than_ind = i;
      break;
    }
  }

  // ... if none found try looping backwards
  if (more_than_ind == -1) {
    for (t_mess_int i = less_than_ind; i != -1; i--) {
      if ((argf = atom_getfloat(argv + i)) > f) {
        more_than_ind = i;
        break;
      }
    }
  }

  // ... if none return
  if (more_than_ind == -1) { return; }

  // Interpolate and output
  x->o_float = (f - atom_getfloat(argv + less_than_ind))
    / (atom_getfloat(argv + more_than_ind) - atom_getfloat(argv + less_than_ind))
    * (more_than_ind - less_than_ind) + less_than_ind;
  lfind_output(x);
}

/****************************************************************
*  Output function
*/
__inline void lfind_output(t_lfind *x)
{
  TRACE("lfind_output");

  outlet_float(x->outl_float, x->o_float);
}

/****************************************************************
*  Setter function for the maxlen attribute
*/
t_max_err lfind_maxlen_set(t_lfind *x, void *attr, long argc, t_atom *argv)
{
  TRACE("lfind_maxlen_set");

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