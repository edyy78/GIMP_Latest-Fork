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

#include "config.h"

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <libgimpbase/gimp-type-module.h>

#include "script-fu-types.h"

#include "script-fu-option.h"

/* Methods for an arg of type SF-OPTION.
 * An SF-OPTION declaration declares a dynamic type inheriting GEnum
 */


GParamSpec *
script_fu_option_get_param_spec (SFScript    *script,
                                 SFArg       *arg,
                                 const gchar *name,  /* Unique name for property. */
                                 const gchar *nick)
{
  GParamSpec *pspec;

  g_debug( "script_fu_option_get_param_spec");

  /* Register enum type so paramspec can reference it. */
  script_fu_option_register_enum(script, arg);

  pspec = g_param_spec_enum (name,
                             nick,
                             arg->label,
                             script_fu_option_get_gtype (script, arg),
                             1,   // int, default, temporarily always 1,
                             // Does an enum start at 1 ???
                             // Was arg->default_value.sfa_enum.history,
                             G_PARAM_READWRITE);
  return pspec;
}

/* Register self's subtype of GEnum into GType runtime type system.
 *
 * Returns the newly registered GType.
 */
GType
script_fu_option_register_enum (SFScript *script, SFArg *arg)
{
  GimpTypeModuleEnum* type_module;
  GString *type_name = script_fu_option_get_type_name(script, arg);

  g_debug( "script_fu_option_register_enum");

  type_module = gimp_type_module_enum_new(type_name->str, "firstvaluename");
  g_type_module_use (G_TYPE_MODULE(type_module));

  g_string_free(type_name, TRUE);

  return script_fu_option_get_gtype (script, arg);
}

/* Returns the previously registered GType. */
GType
script_fu_option_get_gtype (SFScript *script, SFArg *arg)
{
  GType result;

  GString *type_name = script_fu_option_get_type_name(script, arg);

  result = g_type_from_name (type_name->str);
  g_assert(G_TYPE_IS_ENUM (result));

  g_string_free(type_name, TRUE);
  return result;
}

/* Returns unique name for self's subtype of GEnum
 * Cat plugin name with unique arg property name.
 *
 * Caller must g_string_free.
 */
GString *
script_fu_option_get_type_name (SFScript *script, SFArg *arg)
{
  GString *type_name;

  g_debug ("script_fu_option_get_type_name");

  /* Script name is unique in GIMP namespace. */
  type_name = g_string_new(script->name);

  /* Property name is unique in script namespace. */
  type_name = g_string_append(type_name, arg->property_name);
  // default_value.sfa_option.);  /* property name unique within plugin. */
  g_debug ("script_fu_option_get_type_name: %s", type_name->str);
  return type_name;
}
