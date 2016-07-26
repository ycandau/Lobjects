/**
*  @file
*  Lpad - a Max object to pad a list
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
typedef struct _lpad
{
  t_object obj;

  // Inlets, proxies and outlets
  void *inl_proxy_1;
  void *inl_proxy_2;
  void *inl_proxy_3;
  long  inl_proxy_ind;
  void *outl_list;

  // Input variables
  t_mess_int i_pad_left;
  t_atom     i_pad_val[1];

  // Output message
  t_mess_struct o_list[1];

  // Attributes
  t_mess_int maxlen;     // maximum list length
  char       warnings;   // report warnings or not

} t_lpad;

/****************************************************************
*  Global class pointer
*/
static t_class *lpad_class = NULL;

/****************************************************************
*  Function declarations
*/
void *lpad_new      (t_symbol *sym, long argc, t_atom *argv);
void  lpad_free     (t_lpad *x);
void  lpad_assist   (t_lpad *x, void *b, long msg, long arg, char *dst);

void  lpad_bang     (t_lpad *x);
void  lpad_int      (t_lpad *x, t_atom_long n);
void  lpad_float    (t_lpad *x, double f);
void  lpad_list     (t_lpad *x, t_symbol *sym, long argc, t_atom *argv);
void  lpad_anything (t_lpad *x, t_symbol *sym, long argc, t_atom *argv);
void  lpad_clear    (t_lpad *x);
void  lpad_reset    (t_lpad *x);
void  lpad_post     (t_lpad *x);

void  lpad_defaults (t_lpad *x);
void  lpad_action   (t_lpad *x, t_symbol *sym, long argc, t_atom *argv, char offset);
void  lpad_output   (t_lpad *x);

t_max_err lpad_maxlen_set (t_lpad *x, void *attr, long argc, t_atom *argv);

/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  // Initialize frequently used symbols
  sym_init();

  t_class *c;

  c = class_new("Lpad",
    (method)lpad_new,
    (method)lpad_free,
    (long)sizeof(t_lpad),
    NULL, A_GIMME, 0);

  class_addmethod(c, (method)lpad_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)lpad_bang,     "bang",               0);
  class_addmethod(c, (method)lpad_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)lpad_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)lpad_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)lpad_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)stdinletinfo,  "inletinfo", A_CANT,  0);
  class_addmethod(c, (method)lpad_clear,    "clear",              0);
  class_addmethod(c, (method)lpad_reset,    "reset",              0);
  class_addmethod(c, (method)lpad_post,     "post",               0);

  // Define the class attributes
  CLASS_ATTR_INT32    (c, "maxlen", 0, t_lpad, maxlen);
  CLASS_ATTR_ORDER    (c, "maxlen", 0, "1");                    // order
  CLASS_ATTR_LABEL    (c, "maxlen", 0, "maximum list length");  // label
  CLASS_ATTR_SAVE     (c, "maxlen", 0);                         // save with patcher
  CLASS_ATTR_SELFSAVE (c, "maxlen", 0);                         // display as saved
  CLASS_ATTR_ACCESSORS(c, "maxlen", NULL, lpad_maxlen_set);

  CLASS_ATTR_CHAR     (c, "warnings", 0, t_lpad, warnings);
  CLASS_ATTR_ORDER    (c, "warnings", 0, "2");
  CLASS_ATTR_STYLE    (c, "warnings", 0, "onoff");
  CLASS_ATTR_LABEL    (c, "warnings", 0, "report warnings");
  CLASS_ATTR_FILTER_CLIP(c, "warnings", 0, 1);
  CLASS_ATTR_SAVE     (c, "warnings", 0);
  CLASS_ATTR_SELFSAVE (c, "warnings", 0);

  class_register(CLASS_BOX, c);
  lpad_class = c;
}

/****************************************************************
*  Constructor
*/
void *lpad_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_lpad *x = NULL;

  x = (t_lpad *)object_alloc(lpad_class);
  if (!x) { error("Lpad:  Object allocation failed."); return NULL; }

  TRACE("lpad_new");

  // Set inlets, outlets and proxies
  x->inl_proxy_ind = 0;
  x->inl_proxy_3 = proxy_new((t_object *)x, 3L, &x->inl_proxy_ind);
  x->inl_proxy_2 = proxy_new((t_object *)x, 2L, &x->inl_proxy_ind);
  x->inl_proxy_1 = proxy_new((t_object *)x, 1L, &x->inl_proxy_ind);
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
    lpad_maxlen_set(x, NULL, 1, atom);
  }

  // Initialize non attribute variables
  lpad_defaults(x);

  // Process the non attribute arguments, depending on their number
  argc = (t_mess_int)attr_args_offset((short)argc, argv);
  switch (argc) {

  // Zero arguments:  keep default values
  case 0: break;

  // One argument:  output length
  case 1:
    x->o_list->len_cur = CLAMP((t_mess_int)atom_getlong(argv), 0, x->maxlen);
    break;

  // Two arguments:  padding value / output length
  case 2:
    *x->i_pad_val = *argv;
    x->o_list->len_cur = CLAMP((t_mess_int)atom_getlong(argv + 1), 0, x->maxlen);
    break;

  // Three or more:  padding left / padding value / output length
  default:
    x->i_pad_left = CLAMP((t_mess_int)atom_getlong(argv), 0, x->maxlen);
    *x->i_pad_val = *(argv + 1);
    x->o_list->len_cur = CLAMP((t_mess_int)atom_getlong(argv + 2), 0, x->maxlen);
    break;
  }

  lpad_action(x, sym_empty, 0, NULL, 0);
  return x;
}

/****************************************************************
*  Destructor
*/
void lpad_free(t_lpad *x)
{
  TRACE("lpad_free");

  // Free the proxies
  freeobject((t_object *)x->inl_proxy_1);
  freeobject((t_object *)x->inl_proxy_2);
  freeobject((t_object *)x->inl_proxy_3);

  // Free the message structure
  mess_clear(x->o_list);
}

/****************************************************************
*  Assist
*/
void lpad_assist(t_lpad *x, void *b, long msg, long arg, char *dst)
{
  switch (msg) {
  case ASSIST_INLET:
    switch (arg) {
    case 0: sprintf(dst, "list to pad (int, float, symbol, list)"); break;
    case 1: sprintf(dst, "left padding length (int)"); break;
    case 2: sprintf(dst, "padding value (int, float, symbol)"); break;
    case 3: sprintf(dst, "output length (int)"); break;
    default: break;
    }
    break;
  case ASSIST_OUTLET:
    switch (arg) {
    case 0:
      switch (atom_gettype(x->i_pad_val)) {
      case A_LONG:  sprintf(dst, "padded list, with %i (list)", (int)atom_getlong(x->i_pad_val)); break;
      case A_FLOAT: sprintf(dst, "padded list, with %f (list)", atom_getfloat(x->i_pad_val)); break;
      case A_SYM:   sprintf(dst, "padded list, with %s (list)", atom_getsym(x->i_pad_val)->s_name); break;
      }
      break;
    default: break;
    }
    break;
  }
}

/****************************************************************
*  Interface functions
*/
void lpad_bang(t_lpad *x)
{
  TRACE("lpad_bang");

  lpad_output(x);
}

/****************************************************************
*  Process int inputs
*/
void lpad_int(t_lpad *x, t_atom_long n)
{
  TRACE("lpad_int");

  t_atom atom[1];

  switch (proxy_getinlet((t_object *)x)) {
  
  // Left inlet: singleton list
  case 0:
    ASSERT_ALLOC;
    atom_setlong(atom, n);
    lpad_action(x, sym_int, 1, atom, 0);
    lpad_output(x);
    break;

  // Left padding length
  case 1:
    x->i_pad_left = CLAMP((t_mess_int)n, 0, x->maxlen);
    break;

  // Padding value
  case 2:
    atom_setlong(x->i_pad_val, n);
    break;

  // Output length
  case 3:
    x->o_list->len_cur = CLAMP((t_mess_int)n, 0, x->maxlen);
    break;
  }
}

/****************************************************************
*  Process float inputs
*/
void lpad_float(t_lpad *x, double f)
{
  TRACE("lpad_float");

  t_atom atom[1];

  switch (proxy_getinlet((t_object *)x)) {

  // Left inlet: singleton list
  case 0:
    ASSERT_ALLOC;
    atom_setfloat(atom, f);
    lpad_action(x, sym_float, 1, atom, 0);
    lpad_output(x);
    break;

  // Left padding length
  case 1:
    x->i_pad_left = CLAMP((t_mess_int)f, 0, x->maxlen);
    break;

  // Padding value
  case 2:
    atom_setfloat(x->i_pad_val, f);
    break;

  // Output length
  case 3:
    x->o_list->len_cur = CLAMP((t_mess_int)f, 0, x->maxlen);
    break;
  }
}

/****************************************************************
*  Process list inputs
*/
void lpad_list(t_lpad *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("lpad_list");

  switch (proxy_getinlet((t_object *)x)) {

  // Lists should go into the first inlet
  case 0:
    ASSERT_ALLOC;
    lpad_action(x, sym_list, argc, argv, 0);
    lpad_output(x);
    break;

  default:
    ERR("List inputs should go into the first inlet.");
    break;
  }
}

/****************************************************************
*  Process other inputs
*/
void lpad_anything(t_lpad *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("lpad_anything");

  switch (proxy_getinlet((t_object *)x)) {

  // Lists should go into the first inlet
  case 0:
    ASSERT_ALLOC;
    lpad_action(x, sym, argc, argv, 1);
    lpad_output(x);
    break;

  // Padding value
  case 2:
    atom_setsym(x->i_pad_val, sym);
    WARN(argc && x->warnings, "Use a single number or symbol to set the padding value.");
    break;

  default:
    ERR("The inlet expects a number.");
    break;
  }
}

/****************************************************************
*  Clear the list
*
*  Sets the list to 0s without changing the length.
*/
void lpad_clear(t_lpad *x)
{
  TRACE("lpad_clear");

  mess_fill_int(x->o_list, 0, x->o_list->len_cur);
}

/****************************************************************
*  Reset the list
*
*  Sets the list to the padding value without changing the length.
*/
void lpad_reset(t_lpad *x)
{
  TRACE("lpad_reset");

  mess_fill_atom(x->o_list, x->i_pad_val, x->o_list->len_cur);
}

/****************************************************************
*  Post information
*/
void lpad_post(t_lpad *x)
{
  TRACE("lpad_post");

  switch (atom_gettype(x->i_pad_val)) {
  case A_LONG:
    POST("Padding left: %i - Padding value: %i - Output length: %i",
      x->i_pad_left, atom_getlong(x->i_pad_val), x->o_list->len_cur);
    break;
  case A_FLOAT:
    POST("Padding left: %i - Padding value: %f - Output length: %i",
      x->i_pad_left, atom_getfloat(x->i_pad_val), x->o_list->len_cur);
    break;
  case A_SYM:
    POST("Padding left: %i - Padding value: \"%s\" - Output length: %i",
      x->i_pad_left, atom_getsym(x->i_pad_val)->s_name, x->o_list->len_cur);
    break;
  }

  mess_post(x->o_list, "Padded list", x);
}

/****************************************************************
*  Set default values
*/
void lpad_defaults(t_lpad *x)
{
  x->i_pad_left = 0;
  atom_setlong(x->i_pad_val, 0);
  x->o_list->len_cur = x->maxlen;
}

/****************************************************************
*  The specific list action
*/
void lpad_action(t_lpad *x, t_symbol *sym, long argc, t_atom *argv, char offset)
{
  TRACE("lpad_action");

  if (argc + offset + x->i_pad_left > x->maxlen) {
      WARN(x->warnings, "The input message is clipped from length %i to %i.",
        argc + offset, x->maxlen - x->i_pad_left);
  }

  t_atom *list = x->o_list->list;

  // Padding left
  t_mess_int cnt = MIN(x->i_pad_left, x->maxlen);
  for (t_int32 i = 0; i < cnt; i++) { *list++ = *x->i_pad_val; }

  // First atom, if set
  if (offset && ((list - x->o_list->list) < x->maxlen)) { atom_setsym(list++, sym); }

  // Remaining atoms from the list
  cnt = MIN(argc, x->maxlen - (t_mess_int)(list - x->o_list->list));
  for (t_int32 i = 0; i < cnt; i++) { *list++ = *argv++; }

  // Right padding
  cnt = MAX(0, x->maxlen - (t_mess_int)(list - x->o_list->list));
  for (t_int32 i = 0; i < cnt; i++) { *list++ = *x->i_pad_val; }

  mess_set_type(x->o_list);
}

/****************************************************************
*  Output function
*/
__inline void lpad_output(t_lpad *x)
{
  TRACE("lpad_output");

  mess_outlet(x->o_list, x->outl_list);
}

/****************************************************************
*  Setter function for the maxlen attribute
*/
t_max_err lpad_maxlen_set(t_lpad *x, void *attr, long argc, t_atom *argv)
{
  TRACE("lpad_maxlen_set");

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
    lpad_defaults(x);
    lpad_action(x, sym_empty, 0, NULL, 0);
    return MAX_ERR_NONE;
  }
}
