/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpplugin-install.c
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

#include <gio/gio.h>
#include <glib/gstdio.h>

#include "libgimpbase/gimpbase.h"

#include "plug-in-types.h"


static GFile*   gimp_ensure_plug_in_nest_dir  (GFile *source);
static GFile*   gimp_get_plugin_destination   (GFile *plug_in_file);
static gchar *  gimp_get_plug_in_nest_dirpath (GFile *plug_in_file);
static gboolean gimp_copy_file_with_logging   (GFile *source,
                                               GFile *destination);
static void     plugin_admin_error            (gchar *message);
static gboolean gimp_set_file_executable      (GFile *destination);


/* Install a plug-in file into the filesystem.

 * In way that PluginManager expects:
 *    - nested in dir of same name
 *    - permission to execute
 *    - not hidden
 *
 * Into the user's configuration, not system's.
 * GIMP retains the installation on upgrade.
 *
 * Does not query, i.e. does not put plugin in GUI in this session.
 * Can subsequently be queried, now or at next GIMP startup.
 *
 * Allows user to overwrite.
 * Either the nesting dir or the plugin file can exist.
 * This overwrites without warning.
 *
 * This does not guarantee the installed plugin
 * can be queried (register) or work when invoked.
 *
 * Allows shadowing of plugins installed with GIMP.
 * A user can install a changed GIMP plugin script
 * into their user config that takes priority over GIMP's installed plugin.
 *
 * Logs errors to GIMP Console:
 *    - already installed (nesting dir exists)
 *    - filename would be hidden
 *    - other unspecified filesystem errors
 *
 * A nest dir contains a plug-in and has the same name.
 *
 * Returns the installed file. Transfer full.
 * On error, returns NULL.
 */
GFile*
gimp_install_plug_in_file (GFile *plug_in_source_file)
{
  GFile *destination;
  GFile *nest_dir;

  nest_dir =  gimp_ensure_plug_in_nest_dir (plug_in_source_file);
  if (!nest_dir)
    /* Error already declared. */
    return NULL;
  g_free (nest_dir);

  destination = gimp_get_plugin_destination (plug_in_source_file);
  if (!destination)
    /* Failed to form valid path to destination. */
    return NULL;

  if ( !gimp_copy_file_with_logging (plug_in_source_file, destination))
    {
      plugin_admin_error ("Filesystem error during copy.");
      return NULL;
    }

  if (!gimp_set_file_executable (destination))
    {
      plugin_admin_error ("Filesystem error setting execute permission.");
      // TODO clean up
      return NULL;
    }

  return destination;
}

/* Remove plugin's installed file and nesting dir.
 * Should not be called when PluginManager still refers to the installed file.
 */
gboolean
gimp_remove_plug_in_file (GFile *plug_in_installed_file)
{
  gchar *filename;
  gchar *nesting_dir_path;

  g_return_val_if_fail (G_IS_FILE (plug_in_installed_file), FALSE);

  filename = g_file_get_path (plug_in_installed_file);
  nesting_dir_path = gimp_get_plug_in_nest_dirpath (plug_in_installed_file);

  g_debug ("%s %s %s", G_STRFUNC, filename, nesting_dir_path);

  /* Remove the single source file. */
  /* POSIX: FALSE is success. */
  if (g_remove (filename))
    {
      plugin_admin_error ("Failed to delete plugin source file");
      return FALSE;
    }
  // TODO free ?

  /* Remove the nesting dir. */
  /* POSIX: FALSE is success */
  if (g_remove (nesting_dir_path))
    {
      plugin_admin_error ("Failed to delete plugin nesting dir");
      return FALSE;
    }
  else
    {
      g_free (nesting_dir_path);
    }

  /* FUTURE: also remove translation files. */

  return TRUE;
}


static void
plugin_admin_error (gchar *message)
{
  g_debug ("%s error %s", G_STRFUNC, message);
}



/* Returns stem aka root of file name, less suffix.
 * When file is hidden or can't be queried, returns  NULL.
 * Transfer full.
 */
static gchar *
gimp_get_file_stem (GFile *file)
{
  GFileInfo *info;
  gchar     *stem;
  gchar     *ext;

  g_return_val_if_fail ( G_IS_FILE (file), NULL);

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_NAME ","
                            G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                            G_FILE_QUERY_INFO_NONE, /* No flags. */
                            NULL, /* not cancellable. */
                            NULL);


  if ( info == NULL)
    {
      plugin_admin_error ("Source file not exist.");
      return NULL;
    }

  /* Not allow install hidden file. */
  if (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN))
    {
      g_object_unref (info);
      return NULL;
    }

  stem = g_strdup (g_file_info_get_name (info));

  /* Truncate suffix.  Does not affect free. */
  ext = strrchr (stem, '.');
  if (ext)
      *ext = '\0';

  g_object_unref (info);
  return stem;
}

/* Returns a dir path in which to nest a plug-in file.
 * Knows the directory where GIMP puts plug-ins.
 * Returns a path to a dir inside that dir.
 * Dir name same as stem of given plug-in file.
 * Transfer full.
 * Returns NULL when file is a hidden file, or otherwise can't be queried.
 */
static gchar *
gimp_get_plug_in_nest_dirpath (GFile *plug_in_file)
{
  gchar * result;

  gchar *plug_in_file_stem = gimp_get_file_stem (plug_in_file);

  if (!plug_in_file_stem)
    {
      plugin_admin_error ("Source file has no stem.");
      return NULL;
    }

  result = g_build_filename (gimp_directory (),   /* user */
                             "plug-ins",
                             plug_in_file_stem, NULL);

  g_free (plug_in_file_stem);
  g_debug ("%s : %s", G_STRFUNC, result);
  return result;
}


/* Ensure a directory to nest the given plugin source file.
 * Returns NULL when the source file is hidden
 * or other filesystem errors.
 * When the nest dir already exists, returns it.
 * Transfer full.
 */
static GFile*
gimp_ensure_plug_in_nest_dir (GFile* source)
{
  GFile *result = NULL;
  gchar * dirpath = gimp_get_plug_in_nest_dirpath (source);

  if (dirpath == NULL)
    {
      plugin_admin_error ("Source file not exist or hidden.");
      return result;
    }

  // TODO if dir already exists, allow overwrite?

  /* POSIX, returns 0 on success.
   * rwx by owner and group, r others
   */
  if (! g_mkdir (dirpath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
    {
      /* dir now exists, make a GFile.
       * Never fails, but GFile might not support IO if malformed.
       */
      result = g_file_new_for_path (dirpath);
    }
  else
    {
      // TODO see above, prior check for existence
      plugin_admin_error ("Error creating nest dir to install plugin");
    }

  g_free (dirpath);
  return result;
}

/* Return GFile for plugin destination from given path to plugin source.
 * Result: is relative to root, is a file, includes suffix, has identical contents.
 * The result is not the same GFile as the plugin source.
 * Transfer full.
 */
GFile*
gimp_get_plugin_destination (GFile *plug_in_source)
{
  gchar *parent_dirpath = gimp_get_plug_in_nest_dirpath (plug_in_source);
  GFile *result;

  result = g_file_new_build_filename (parent_dirpath,
                                      g_file_get_basename (plug_in_source),
                                      NULL);

  g_debug ("%s : %s", G_STRFUNC, g_file_get_parse_name (result));
  return result;
}

/* Copies source file to destination file.
 * Ownership is not changed.
 * Copies target of symlink.
 *
 * Requires destination is a leaf file, not a dir.
 *
 * Copy with overwrite.
 * since we fail earlier if nesting directory already exists TODO
 */
static gboolean
gimp_copy_file_with_logging (GFile *source, GFile *destination)
{
  gboolean did_succeed;
  GError  *error = NULL;

  did_succeed = g_file_copy (source, destination,
                        G_FILE_COPY_ALL_METADATA | G_FILE_COPY_OVERWRITE,
                        NULL, /* not cancellable */
                        NULL, NULL, /* no progress_callback */
                        &error);
  if (!did_succeed)
    {
      /* Log specific filesystem error.
       * A more general error is said by caller.
       */
      g_warning ("%s error %s", G_STRFUNC, error->message);
      g_free (error);
    }

  return did_succeed;
}

/* Set permission executable.
 * See GLib docs about Windows.
 *
 * FUTURE no reason to require is executable, fix PluginManager.
 */
static gboolean
gimp_set_file_executable (GFile *destination)
{
  /* POSIX, 0 on success, invert the logic. */
  return !g_chmod (g_file_get_path (destination), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}