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

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "actions-types.h"

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"

#include "widgets/gimpcontainerview.h"
#include "widgets/gimpimageview.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpeditor.h"
#include "widgets/gimpaction.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"

#include "actions/images-actions.h"  /* images-actions-update */
#include "images-commands.h"


/*  public functions */

void
images_raise_views_cmd_callback (GimpAction *action,
                                 GVariant   *value,
                                 gpointer    data)
{
  GimpContainerEditor *editor = GIMP_CONTAINER_EDITOR (data);
  GimpContainer       *container;
  GimpContext         *context;
  GimpImage           *image;

  container = gimp_container_view_get_container (editor->view);
  context   = gimp_container_view_get_context (editor->view);

  image = gimp_context_get_image (context);

  if (image && gimp_container_have (container, GIMP_OBJECT (image)))
    {
      GList *list;

      for (list = gimp_get_display_iter (image->gimp);
           list;
           list = g_list_next (list))
        {
          GimpDisplay *display = list->data;

          if (gimp_display_get_image (display) == image)
            gimp_display_shell_present (gimp_display_get_shell (display));
        }
    }
}

void
images_new_view_cmd_callback (GimpAction *action,
                              GVariant   *value,
                              gpointer    data)
{
  GimpContainerEditor *editor = GIMP_CONTAINER_EDITOR (data);
  GimpContainer       *container;
  GimpContext         *context;
  GimpImage           *image;

  container = gimp_container_view_get_container (editor->view);
  context   = gimp_container_view_get_context (editor->view);

  image = gimp_context_get_image (context);

  if (image && gimp_container_have (container, GIMP_OBJECT (image)))
    {
      gimp_create_display (image->gimp, image, GIMP_UNIT_PIXEL, 1.0,
                           G_OBJECT (gimp_widget_get_monitor (GTK_WIDGET (editor))));

      /* Sensitize actions: delete now disabled. */
      images_actions_update (gimp_action_get_group (action), data);
    }
}

/* Callback from action "images-delete".
 *
 * data is an editor having a ui (menu or button) that user chose to cause action.
 * The editor is the "Images" dockable.
 * This is NOT the "view-close" action from: the main menu,
 * the context popup menu of the main canvas, or a X button in image tab.
 *
 * Remove image from the editor's container.
 * When the image has no displays, dispose of the image.
 * The image is expected to not have a display.
 * (The action is disabled when the image does have a display.)
 * This can happen when an image is created by a plugin or an interpreter console.
 *
 * Other widgets that view components of the image can reference the image,
 * but they must be weak pointers.
 *
 * The action name "delete" is somewhat confusing because it is like "close".
 * It does NOT remove any files.
 * It only removes an icon of the image from the editor's viewed list,
 * and removes the image from memory.
 *
 * The action "delete" is directly by a user.
 * The action is NOT a plugin calling API gimp-image-delete.
 */
void
images_delete_image_cmd_callback (GimpAction *action,
                                  GVariant   *value,
                                  gpointer    data)
{
  GimpContainerEditor *editor = GIMP_CONTAINER_EDITOR (data);
  GimpContainer       *container;
  GimpContext         *context;
  GimpImage           *image;

  container = gimp_container_view_get_container (editor->view);
  context   = gimp_container_view_get_context (editor->view);

  image = gimp_context_get_image (context);
  g_debug ("%s is_dirty %i", G_STRFUNC, gimp_image_is_dirty (image));

  if (image && gimp_container_have (container, GIMP_OBJECT (image)))
    {
      gimp_container_remove (container, image);

      if (gimp_image_get_display_count (image) == 0)
        /* There is no image_delete.  This will cause image_dispose. */
        g_object_unref (image);
    }
}
