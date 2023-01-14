
#pragma once

/* Function producing an int combo box widget for a particular GIMP object type. */
typedef GtkWidget* (*ComboBoxWidgetCreator)(GimpImageConstraintFunc func, gpointer data, GDestroyNotify func2);

/* Generic function types: we cast from a specific type to this generic type.
 * Allowing generic implementation while voiding compiler warnings.
 */

/* Function returning a pointer to a GIMP object from its integer ID. */
typedef gpointer (*FuncIDToObject)(gint ID);

/* Function returning an integer ID from a GIMP object pointer. */
typedef gint (*FuncObjectToID)(gpointer obj);

GtkWidget *gimp_prop_widget_factory (ComboBoxWidgetCreator func,
                                     GObject     *config,
                                     const gchar *property_name,
                                     const gchar *label,
                                     FuncIDToObject func_id_to_obj,
                                     FuncObjectToID func_obj_to_id);
