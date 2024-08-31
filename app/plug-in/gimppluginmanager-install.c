/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-2002 Spencer Kimball, Peter Mattis, and others
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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gegl.h>

#include "libgimpbase/gimpbase.h"

#include "plug-in-types.h"

#include "gimpplugindef.h"
#define __YES_I_NEED_GIMP_PLUG_IN_MANAGER_CALL__
#include "gimppluginmanager.h"
#include "gimppluginmanager-restore.h"
#include "gimppluginmanager-call.h"
#include "gimppluginprocedure.h"

#include "gimppluginmanager-install.h"


/* Returns GStrv of menu labels of user installed plugins.
 * GStrv empty when user has not installed any plugins.
 */
GStrv
gimp_plug_in_manager_get_user_menu_labels (GimpPlugInManager *manager)
{
  GStrv         result;
  GStrvBuilder *builder;

  builder = g_strv_builder_new ();

  g_debug ("%s", G_STRFUNC);

  /* Filter all installed, non file-load procedures, matching on:
   * grandparent dir is user.config.plug-ins dir
   */
  for (GSList *proc_list = manager->plug_in_procedures; proc_list; proc_list = proc_list->next)
    {
      GimpPlugInProcedure *proc = proc_list->data;

      /* Up two dirs from file. */
      GFile *parent_of_nesting_dir = g_file_get_parent (g_file_get_parent (proc->file));

      gchar *user_install_dir_path = g_build_filename (gimp_directory (), "plug-ins", NULL);
      GFile *user_plugin_install_dir = g_file_new_for_path (user_install_dir_path);

      if (g_file_equal (parent_of_nesting_dir, user_plugin_install_dir))
        {
          g_debug ("%s match %s %s", G_STRFUNC, g_file_get_path(proc->file), proc->menu_label);

          g_strv_builder_add (builder, proc->menu_label);
        }
    }

  result = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  return result;
}

/* Returns installed file of plugin for given menu_label.
 *
 * Menu labels are unique and one-to-one with actions.
 * Menu label is one-to-one with procedure.
 * A plugin file is one-to-many with procedure/menu_label.
 * Removing the returned file may remove more than just the given menu_label.
 *
 * Returns a new GFile.  Transfer full.
 */
GFile*
gimp_plug_in_manager_get_file_by_menu_label (GimpPlugInManager *manager, const gchar *menu_label)
{
  GFile *result = NULL;

  g_debug ("%s %s", G_STRFUNC, menu_label);

  /* Filter all installed, non-file procedures, matching on: menu_label */
  for (GSList *proc_list = manager->plug_in_procedures; proc_list; proc_list = proc_list->next)
    {
      GimpPlugInProcedure *proc = proc_list->data;

      if (g_strcmp0 (proc->menu_label, menu_label) == 0)
        {
          g_debug ("%s match %s %s", G_STRFUNC, g_file_get_path(proc->file), proc->menu_label);
          /* Dupe, since manager owns the GFile and may free it. */
          result = g_file_dup (proc->file);
          break;
        }
    }

  return result;
}



/* Fill given PlugInDef with currently installed procedures
 * that are defined in the def's file.
 *
 * Compare to use of def at query time: recovers the same, discarded def,
 * but incompletely initialized.
 *
 * Side effects on def will be freed when the def is freed.
 */
static void
gimp_plug_in_manager_fill_def (GimpPlugInManager *manager,
                               GimpPlugInDef     *def)
{
  g_debug ("%s", G_STRFUNC);

  (void) gimp_plug_in_manager_get_user_menu_labels (manager); // TEST

  /* Filter installed, non-file procedures, matching on def's file. */
  for (GSList *proc_list = manager->plug_in_procedures; proc_list; proc_list = proc_list->next)
    {
      GimpPlugInProcedure *proc = proc_list->data;

      if (g_file_equal (proc->file, def->file))
        {
          g_debug ("%s match %s", G_STRFUNC, g_file_get_path(proc->file));
          gimp_plug_in_def_add_procedure (def, proc);
        }
    }
}

/* Remove a plugin identifed by given file.
 * Requires the file is installed in GIMP directory.
 */
gboolean
gimp_plug_in_manager_remove_plugin (GimpPlugInManager *manager, GFile *file)
{
  GimpPlugInDef *definition = gimp_plug_in_def_new (file);

  g_debug ("%s", G_STRFUNC);

  /* Fill definition with installed procedures. */
  gimp_plug_in_manager_fill_def (manager, definition);

  /* remove the plugin's procedures from manager and PDB. */
  for (GSList *proc_list = definition->procedures; proc_list; proc_list = proc_list->next)
    {
      GimpPlugInProcedure *proc = proc_list->data;

      g_debug ("%s removing %s", G_STRFUNC, proc->menu_label);

      gimp_plug_in_manager_remove_procedure (manager, GIMP_PROCEDURE (proc));
      /* Manager removed proc from it's list and unreffed the proc.
       * Manager removed proc from PDB.
       * Manager updated menus.
       */
    }

  g_object_unref (definition);
  return TRUE;
}

/* Query and install a single file.
 * Requires the file is previously installed in GIMP.
 */
void
gimp_plug_in_manager_install_plugin (GimpPlugInManager *manager,
                                     GimpContext       *context,
                                     GFile             *file)
{
  GimpPlugInDef *definition = gimp_plug_in_def_new (file);

  /* Fill definition with defined procedures, by querying the def's file. */
  gimp_plug_in_manager_call_query (manager, context, definition);

  /* add the plugin's procedures to manager and to PDB. */
  for (GSList *proc_list = definition->procedures; proc_list; proc_list = proc_list->next)
    {
      gimp_plug_in_manager_add_procedure (manager, proc_list->data);
      gimp_plug_in_manager_add_to_db (manager, context, proc_list->data);
    }
}

