/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


/*
 * Implements late binding of calls to a special foreign function.
 * A mod of TinyScheme.
 * The mod is special to GIMP.
 * The special foreign func calls the PDB
 * i.e. ultimately calls gimp_pdb_run_procedure(procedure name, args)
 *
 * This uses more than the usual TinyScheme support for foreign functions.
 * i.e. we copy certain C definitions from scheme.c
 * to augment what is in scheme-private.h.
 * That copied code must be kept in correspondence with the original
 * (or we could modify TinyScheme further, to export the copied definitions.)
 */

#include "config.h"

#include <string.h>
#include <glib.h>

#include "libgimp/gimp.h"

#include "tinyscheme/scheme-private.h" /* type "scheme" */

#include "scheme-wrapper.h" /* script_fu_marshal_procedure_call */
#include "script-fu-errors.h" /* G_LOG_DOMAIN */
#include "script-fu-compat.h" /* is_deprecated */
#include "script-fu-late-bind.h"


/* This section is a hack to avoid touching tinyscheme source.
 * Which doesn't expose things we need.
 * !!! Must correspond to definitions in tinyscheme source.
 */
 /* C macros not exported by scheme-private.h */
#define T_ATOM       16384
#define typeflag(p)      ((p)->_flag)
#define is_atom(p)       (typeflag(p)&T_ATOM)

#ifndef USE_SCHEME_STACK
/* Not exposed by scheme.h */
struct dump_stack_frame {
  enum scheme_opcodes op;
  pointer args;
  pointer envir;
  pointer code;
};
#endif


/* local */
static void      _bind_symbol_to_script_fu_wrapper_foreign_func (scheme  *sc,
                                                                 gchar   *symbol_name);

static pointer   _new_atom_for_bound_string                     (scheme  *sc,
                                                                 pointer  binding);

static void      _push_onto_dump_args                           (scheme  *sc,
                                                                 pointer  value);

static gboolean  _is_call_to_PDB                                (pointer  binding);
static gboolean  _procedure_seems_in_PDB_and_not_SF             (gchar *  symbol_name);
static gboolean  _procedure_seems_in_PDB                        (gchar *  symbol_name);


/* We only create a single foreign func, and keep a reference.
 * The func is foreign to TinyScheme, and wraps all PDB procedures.
 */
static pointer script_fu_wrapper_foreign_func=0;



/*
 * Given an unbound scheme symbol, try to bind it to a foreign function that calls a PDB procedure.
 * Also binds deprecated PDB names.
 *
 * Formerly scheme-wrapper.c defined all PDB procedure names as symbols, early, at initialization.
 *
 * Binding is indirect: to a Scheme foreign function that calls PDB.run_procedure(called_name).
 *
 * Returns whether a binding was done.
 * Side effects on bindings in the global env.
 */
gboolean
try_late_bind_symbol_to_foreign_func (scheme   *sc,
                                      gchar    *symbol_name)
{
  gboolean result = FALSE;
  gchar  *new_name;

  if (_procedure_seems_in_PDB_and_not_SF (symbol_name))
    {
      /* Name exists in PDB OR is deprecated and a replacement exists in PDB.
       * When deprecated, the bound foreign function wrapper will convert to new name.
       * A script usually uses the deprecated name or the new name consistently,
       * but it could use a mix and this still works, with two separate bindings.
       */
      g_info ("Symbol %s bound to PDB.\n", symbol_name);
      _bind_symbol_to_script_fu_wrapper_foreign_func (sc, symbol_name);
      result = TRUE;
    }
  else
    {
      g_info ("Symbol %s not bindable to PDB.\n", symbol_name);
      result = FALSE;
    }

  return result;
}


/*
 * Called while evaluating a symbol.
 * If the symbol is bound to a special foreign func, evaluate to multiple atoms,
 * else return the usual bound value of the symbol.
 * This is not generic to late-binding, but special for ScriptFu:
 * the bound foreign func needs another argument.
 *
 * Expand a binding to a PDB name into two atoms suitable for sc->args.
 * First atom is a foreign function, the wrapper that calls PDB.
 * Second atom is a string for the PDB name (extra arg to the wrapper.)
 *
 * Side effect on the inner interpreter internals i.e. 'sc'.
 * First atom, the foreign function, is pushed onto previous frame's args.
 * Second atom, the bound name, is returned, caller will push onto sc->args.
 * Note args are temporarily kept in reverse order.
 *
 * This is not a Scheme macro, but does alter the normal evaluation.
 * Maybe it could be implemented more purely as a Scheme macro.
 */
pointer
value_for_binding (scheme *  sc,
                   pointer   binding)
{
  pointer result_value;
  pointer bound_value;

  /* slot_value_in_env not exported by TinyScheme, equivalent is cdr(binding) */
  bound_value = pair_cdr (binding);

  if (_is_call_to_PDB (binding))
    {
      g_assert (is_foreign (bound_value));
      _push_onto_dump_args (sc, bound_value);

      /* Result value is bound name.
       * Return it, caller will push onto dump.args.
       */
       result_value = _new_atom_for_bound_string (sc, binding);
       g_assert (is_atom (result_value));
    }
  else
    {
      result_value = bound_value;
    }

  /* Ensure returned value is an atom (fully evaluated), caller will push it onto sc->args.
   * Ensure (not is_call_to_PDB AND result is bound value)
   * OR (is_call_to_PDB AND result is_string, AND result is_atom) plus side effect on frame args
   */

  return result_value;
}


/* local functions */


/* Does the given name seem in the PDB?
 * Seem means:
 * - deprecated name, which we will translate to a replacement name
 * - canonically named (looks like a PDB name)
 *
 * Not: actually in the PDB.
 * If not exist, get an error later in foreign function.
 */
static gboolean
_procedure_seems_in_PDB (gchar * symbol_name)
{
  gchar * new_name;

  /* Faster to first check deprecated?
   * Thats a local search O(n) (but could be improved.)
   * Call to GIMP is relatively expensive.
   * Then check canonical: must be canonical to be a GIMP name.
   *
   * FUTURE query the PDB for all names.
   * Keep a fast dictionary of existing and deprecated names.

   * Not used, this fails since v3 this requires is_canonical:
   * && gimp_pdb_procedure_exists (gimp_get_pdb (), symbol_name)
   */

  return ( is_deprecated (symbol_name, &new_name) ||
           gimp_is_canonical_identifier (symbol_name));
  /* Discard new_name. */
}



/* Does the given name seem in the PDB,
 * AND is not a canonically named ScriptFu script
 * since those are already loaded into the ScriptFu extension
 * that is, already bound to a Scheme text, not a foreign function.
 */
static gboolean
_procedure_seems_in_PDB_and_not_SF (gchar * symbol_name)
{
  /* Faster to first check not have prefix "script-fu"
   * Call to _procedure_seems_in_PDB is more expensive.
   */
  return ((! strncmp ("script-fu", symbol_name, strlen ("script-fu")) == 0) &&
           _procedure_seems_in_PDB (symbol_name) );
}


/* Is binding from a name seeming in the PDB?
 * Which will always be to a wrapper foreign function script_fu_marshal_procedure_call.
 */
static gboolean
_is_call_to_PDB (pointer binding)
{
  /* A binding in an env is a (symbol . value) pair */
  pointer bound_value = pair_cdr (binding);

  /* Faster to first check if binding to any foreign func. */
  if (is_foreign (bound_value) )
    {
      pointer bound_symbol = pair_car (binding);

      /* Not every foreign function is a call to the PDB:
       * 1) script-fu-register etc. is implemented in C code in script-fu-wrapper
       * 2) script files defining script-fu-foo are read as text into ScriptFu extension,
       * and ScriptFu interprets them without calling PDB
       * even though they are also names in the PDB.
       */
      return _procedure_seems_in_PDB_and_not_SF (symname (bound_symbol));
    }
  else
    return FALSE;
}


/*
 * Design alternatives:

 * A call to the PDB goes through one foreign func,
 * but the foreign func requires the name of the called PDB procedure.
 *
 * 1) Bind all symbols calling the PDB to the same wrapper foreign func,
 * and convey the name of the symbol to the wrapper out of band, i.e. not an arg.
 *
 * 2) Bind each symbol to the PDB to its own partial parameterized wrapper foreign func.
 * i.e. (foo args ) is bound to a small script (-gimp-proc-db-call "foo" args)
 * That was the previous design.
 */


 /* Bind symbols for PDB procedure names to a same foreign function,
  * a wrapper that ultimately calls PDB.run_procedure( procedure_name ).
  */
static void
_bind_symbol_to_script_fu_wrapper_foreign_func (scheme   *sc,
                                                gchar * symbol_name)
{
  /* !!! The symbol is not passed separately, but is in the sc. */
  pointer symbol = sc->code;

  g_info ("late bind symbol %s\n", symbol_name);

  if (!script_fu_wrapper_foreign_func)
    script_fu_wrapper_foreign_func =
      sc->vptr->mk_foreign_func (sc, script_fu_marshal_procedure_call);

  /* Symbol already exists.  No need to make it. But ensure immutable. */
  sc->vptr->scheme_define (sc,
                           sc->global_env,
                           symbol,
                           script_fu_wrapper_foreign_func); /* the atom to bind symbol to. */
  sc->vptr->setimmutable (symbol);
}


/* Make a new string atom, separate but identical to the one in the binding.
 * A binding is from a string to a value.
 * Usually evaluation computes an atom from the value.
 * Here, we new an atom using the bound string.
 *
 * FUTURE it might be possible to use the string atom in the binding,
 * since the binding in the global env will not go away.
 * Since we are putting it in a list of evaluated args,
 * which will go out of scope, our copy will get garbage collected.
 * The one in the binding will never be garbage collected.
 */
static pointer
_new_atom_for_bound_string (scheme *  sc,
                            pointer   binding)
{
  gchar * bound_string;

  /* car of binding is-a symbol.
  * car of symbol is-a scheme string.
  * !!! But its not an atom, and we don't own it.
  */
  g_assert (is_string (pair_car (pair_car (binding))));
  /* A scheme string is a cell of a particular type, not a char *. */
  bound_string = string_value (pair_car (pair_car (binding)));
  return mk_string (sc, bound_string);
  /* Result owned by interpreter, to be garbage collected later. */
}


/*
 * We are in midst of evaluating a symbol.
 * Prepend given value onto the list of args in evaluation stack in the previous frame.
 * Only used for special case: insert an arg needed by a foreign function.
 *
 * Not pretty, understands too much about the evaluation process.
 * Ideally, we would not change the history of evaluation.
 */
static void
_push_onto_dump_args (scheme *sc,
                      pointer value)
{

  /* USE_SCHEME_STACK is a compile time option.
   * GIMP does use the scheme stack.
   * Experiments show that not using the scheme stack gains little performance.
   */
#ifndef USE_SCHEME_STACK
  /* sc->dump is declared a "pointer" but stores int count of frames
   * sc->dump_base is array of dump_stack_frame (i.e. pointer to dump_stack_frame)
   * dump_stack_frame.args is a list of atoms
   */

  /* assert there is previous frame at -1 */
  struct dump_stack_frame *frame;
  gsize nframes = (int)sc->dump - 1;
  frame = (struct dump_stack_frame *)sc->dump_base + nframes;

  /* When not USE_SCHEME_STACK, frame is a struct.
   * frame->args is not a cell, only a pointer to cell.
   * Pointed to cell is first member of args list.
   * Replace frame->args with a pointer to first cell of prepended list.
   */
  frame->args = cons (sc, value, frame->args);

#else
  /* When USE_SCHEME_STACK, frame is a list.
   * sc->dump is the frame.
   * Second cell of frame is pointer to list of args.
   * Replace it's car with a pointer to list that is original with prepended value.
   * Note left side must be an lvalue, that is address of a struct field.
   */
  pair_cdr (sc->dump)->_object._cons._car = cons (sc, value, pair_car (pair_cdr (sc->dump)));
#endif
}
