/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gegl.h>
#include <gtk/gtk.h>

#include "gimpwidgets.h"
#include "gimpmockenum.h"

/* Parsing error messages are for programmers, not localized. */

/* MockEnum
 *
 * An enumeration described in the blurb of a GParamSpec.
 * Not a true type, only used to create combo box widgets.

 * The combo box enforces membership in the enum.
 * The programmer must ensure that the min and max of the GParamSpec
 * agree with the min and max values of the mock enum.
 * The code here does not alter the passed GParamSpec.
 *
 * Like GEnum is a named set of GEnumValue,
 * mock enum is a named set of named values.
 *
 * BNF for the format of the blurb that specifies a mock enum:
 *
 * MockEnumString ::= EnumName "{" EnumValueList "}"
 * EnumValueList ::= EnumValue | EnumValue ", " EnumValueList
 * EnumValue ::= EnumValueName "(" EnumValueValue ")"
 * EnumValueName ::= any character except "(", but usually alphanumeric and whitespace
 * EnumValueValue ::= numeric characters except ")"
 *
 * The parsing routines are named for each construct.
 *
 * Example:  Foo Enum {Bar Value(0), Zed,Value(1)}
 * Will appear as an into combo box labeled "Foo Enum"
 * having choices "Bar Value" and "Zed,Value"
 * The character '(' cannot be used in a value name.
 */

/* We assume this is called only from a plugin using GimpProcedureDialog.
 * Methods are not exported from libgimpwidgets.
 * If it crashes, it affects only the plugin.
 * Parse errors appear in the console.
 */

 typedef struct
 {
   const gchar *name;
   gint         value;
 } MockEnumValue;

/* Note blurb and nick are one and the same? */

/*
 * Parsing methods.
 *
 * Usually return a string, and update the remainder.
 * The remainder is suffix of the source string after the delimiter.
 * The delimiters are not in either the returned token or the remainder
 * (see g_strsplit)
 *
 * Memory allocation:
 * Caller must free returned strings: the result and the remainder.
 * Most parsing methods free the passed source.
 *
 * No parsing method modifies strings, they are const.
 */


/* Utility parsing methods. */

/* Free the remainder, then replace it. */
static void
free_then_update_remainder (const gchar **remainder, const gchar *new_remainder)
{
  g_free ((gpointer) *remainder);  /* Safe if *remainder is NULL */
  *remainder = new_remainder;
}

/* Return the remainder (becomes the new source) then NULL it. */
static const gchar*
shift_remainder_then_null (const gchar** remainder)
{
  const gchar * result = *remainder;
  *remainder = NULL;
  return result;
}

static void
parse_error (const gchar* msg)
{
  g_warning("Error parsing mock enum in param spec blurb: %s", msg);
}


/* Local parsing methods. */

/* Returns the "enum name" from the blurb.
 * Prefix up to "{"
 *
 * !!! Source is owned by the caller, and is not freed.
 *
 * Remainder is allocated here and caller must freed it.
 */
static const gchar *
parse_enum_name (const gchar *source, const gchar **remainder)
{
  const gchar * result;

  gchar **tokens = g_strsplit (source, "{", 2);
  /* !!! Not free the source, it is owned by the caller. */
  result = g_strdup(tokens[0]);
  free_then_update_remainder (remainder, g_strdup(tokens[1]));
  g_debug("enum name: %s", result);
  g_strfreev(tokens);

  /* When not find "{", remainder is NULL and result is entire source. */
  return result;
}

/* Returns an "enum value name", the prefix of source up to "("
 *
 * Returns NULL if not parsed.
 *
 * Source is freed.
 * Caller must free the string.
 */
static const gchar *
parse_enum_value_name (const gchar *source, const gchar **remainder)
{
  const gchar *result = NULL;

  /* source can be NULL */

  gchar **tokens = g_strsplit (source, "(", 2);
  result = g_strdup(tokens[0]);
  g_free ((gpointer)source);
  free_then_update_remainder (remainder, g_strdup(tokens[1]));
  g_strfreev(tokens);

  /* If the remainder is NULL, we didn't find a "(". */
  if (*remainder == NULL)
    {
      parse_error ("Expected '('");
      g_free ((gpointer)result);
      result = NULL;
    }
  /* else returning a valid enum value name. */

  g_debug ("enum value name: %s", result);
  return result;
}

/* Returns an "enum value value" a string, "1" from e.g. "1)"
 *
 * Returns NULL if not parsed.
 *
 * Caller must free the string.
 */
static const gchar *
parse_enum_value_value (const gchar *source, const gchar **remainder)
{
  const gchar * result = NULL;

  g_debug("parse_enum_value_value: source: %s", source);

  if (source != NULL)
    {
      /* Assert we just parsed a "(" */

       /* Parse up to  ) */
      gchar **tokens = g_strsplit (source, ")", 2);
      g_free ((gpointer)source);
      result = g_strdup(tokens[0]);
      free_then_update_remainder (remainder, g_strdup(tokens[1]));
      g_strfreev(tokens);

      /* If the remainder is NULL, we didn't find ). */
      if (*remainder == NULL)
        {
          parse_error ("Expected ')'");
          g_free ((gpointer)result);
          result = NULL;
        }
    }
  /* else return NULL. */

  /* assert remainder is NULL, or a string not starting with ")"
   * Expect either another enum value ", foo(1)" or "}"
   */

  g_debug("enumvalue value is %s", result);
  return result;
}

/* Returns MockEnumValue
 *
 * It has NULL name if not parsed.
 *
 * Source is freed.
 * Caller must free the string of the MockEnumValue.
 */
static MockEnumValue
parse_enum_value (const gchar *source, const gchar **remainder)
{
  MockEnumValue result = {.name = NULL, .value = -1};

  result.name  = parse_enum_value_name (source, remainder);
  if (result.name != NULL)
    {
      /* We found a name and ate a '(' */
      const gchar *new_source = shift_remainder_then_null (remainder);
      const gchar *enum_value_value = parse_enum_value_value(new_source, remainder);

      if (*remainder == NULL)
        {
          g_warning ("Expected int literal");
          g_free ((gpointer)enum_value_value);
          /* Free the name. */
          g_free ((gpointer)result.name);
          result.name = NULL;
        }
      else
        {
          /* We found a string and ate a ')' */
          /* convert string to int, the string must be locale independent, i.e. ascii. */
          /* atoi returns 0 for all whitespace or empty string. */
          gint value = atoi(enum_value_value); // g_ascii_strtoll (enum_value_value);
          result.value = value;
          g_free ((gpointer)enum_value_value);
        }
    }
  else
    {
      /* Not an error, but no more "(", i.e. enum_values
       * Expect a terminating "}"
       */
    }

  return result;
}

/* Expect an EnumValue e.g. "foo(1)"
 * Give warning when not find.
 *
 * Return success.
 *
 * Frees the source.
 */
static gboolean
parse_enum_value_and_append_to_store (const gchar  *source,
                                      const gchar **remainder,
                                      GimpIntStore *store)
{
  MockEnumValue enum_value;
  gboolean result;

  g_assert (*remainder == NULL);
  g_debug ("parse_enum_value_and_append_to_store");

  enum_value = parse_enum_value(source, remainder);
  /* assert source was freed. */

  if (enum_value.name != NULL)
    {
      /* Cast name to char* to avoid compiler warning. */
      gimp_int_store_append((GtkListStore *)store, enum_value.name, enum_value.value);
      g_free ((gpointer)enum_value.name);
      result = TRUE;
    }
  else
    {
      parse_error("Expected enum value.");
      result = FALSE;
    }
  /* assert source was freed. */
  return result;
}

/* Eat separator from source.
 * return whether found.
 *
 * Behave like a strsplit: returns suffix of source in remainder.
 *
 * Caller owns the remainder.
 */
static gboolean
parse_eat_separator (const gchar *source, const gchar **remainder)
{
  g_assert (*remainder == NULL);

  /*  */
  if (*source == ',' && *(source+1) == ' ')
    {
      *remainder = g_strdup(source+2);
      g_free ((gpointer)source);
      return TRUE;
    }
  else
    {
      *remainder = source;
      return FALSE;
    }
  /* assert *remainder needs free, but source does not. */
}

/* Parse list of enum_value pairs, and append them to the store. */
static void
parse_enum_value_list_into_store (const gchar *source, GimpIntStore *store)
{
  const gchar *remainder = NULL;

  /* A list starts with an enum_value. */
  if (parse_enum_value_and_append_to_store (source, &remainder, store))
    {

      /* Optionally followed by separator and more enum_values */
      while (TRUE)
        {
          const gchar *separator_source = shift_remainder_then_null (&remainder);
          if (!parse_eat_separator(separator_source, &remainder))
            {
              /* No separator is not an error, but expect next char '}' */
              break;
            }
          else
            {
              /* Expect enum_value after separator */
              const gchar *enum_value_source = shift_remainder_then_null (&remainder);
              if (! parse_enum_value_and_append_to_store (enum_value_source, &remainder, store))
                {
                  parse_error( "Expected enum value after comma");
                  break;
                }
            }
        }
    }

    /* If remainder is not '}', parsing error. */
    if (*remainder != '}') parse_error("Expected '}'");
    g_free ((gpointer)remainder);
}

/* Return an int store parsed from the source.
 *
 * Frees the source.
 */
static GimpIntStore*
gimp_mock_enum_get_store_from_blurb(const gchar *source)
{
  GimpIntStore *result;

  result = (GimpIntStore *) gimp_int_store_new_empty ();
  parse_enum_value_list_into_store (source, result);
  /* assert source was freed. */

  return result;
}


/* Public methods. */

/* Does the blurb of the pspec indicate a mock enum */
gboolean
gimp_mock_enum_is_mock (GParamSpec *pspec)
{
  const gchar  *blurb;
  gboolean      result;

  blurb = g_param_spec_get_blurb (pspec);

  /* Find two of the expected delimiter chars. */
  if (g_strrstr(blurb, "{") != 0 && g_strrstr(blurb, "(") != 0)
    result = TRUE;
  else
    result = FALSE;

  /* Not free blurb; is owned by pspec instance. */

  g_debug ("gimp_mock_enum_is_mock: %s %d", blurb, result);
  return result;
}

/* Return a labeled int combo box having a store parsed from the blurb of a pspec. */
GtkWidget *
gimp_mock_enum_get_widget (GObject  *config, const gchar *property, GParamSpec *pspec)
{
  GtkWidget    *widget;
  GimpIntStore *store;
  const gchar  *label;
  const gchar  *remainder = NULL;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  /* Label for the widget is the enum name. */
  label = parse_enum_name (g_param_spec_get_blurb (pspec), &remainder);

  /* Expect the remainder is a list of enum_value pairs.
   * Remainder becomes source. */
  store = gimp_mock_enum_get_store_from_blurb (remainder);
  /* assert remainder was freed. */

  /* Widget is a pop-up menu. */
  widget = gimp_prop_int_combo_box_new (config, property, store);

  gtk_widget_set_vexpand (widget, FALSE);
  gtk_widget_set_hexpand (widget, TRUE);

  /* Wrap combo box with label. */
  return gimp_label_int_widget_new (label, widget);
}
