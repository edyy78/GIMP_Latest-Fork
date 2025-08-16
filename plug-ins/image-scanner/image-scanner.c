/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * image-scanner - Image scannerplug-in for the GIMP that interfaces
 *                 with SANE to scan images into GIMP.
 * Copyright (C) 2025  Draekko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <locale.h>
#include <sane/sane.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

#include "image-scanner.h"

#include <gtk-3.0/gtk/gtkcombobox.h>
#include <gtk-3.0/gtk/gtklabel.h>
#include <gtk-3.0/gtk/gtkprogressbar.h>
#include <gtk-3.0/gtk/gtkradiobutton.h>
#include <gtk-3.0/gtk/gtkrange.h>
#include <gtk-3.0/gtk/gtkscale.h>
#include <gtk-3.0/gtk/gtkspinner.h>
#include <gtk-3.0/gtk/gtktreemodel.h>
#include <gtk-3.0/gtk/gtktypes.h>
#include <gtk-3.0/gtk/gtkwidget.h>
#include <sys/wait.h>

GType imagescanner_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ImageScanner, imagescanner, GIMP_TYPE_PLUG_IN)
GIMP_MAIN (IMAGE_SCANNER_TYPE)
DEFINE_STD_SET_I18N

static void
imagescanner_init (ImageScanner *image_scanner)
{
}

static void
image_scanner_query_sane_for_devices()
{
  SANE_Status status;

  devices = 0;

  sane_init (0, 0);
  status = sane_get_devices ((const SANE_Device ***)&device_list, SANE_FALSE);
  if (status != SANE_STATUS_GOOD) {
      g_print (_("sane_get_devices() failed: %s\n"),
           sane_strstatus (status));
  } else {
      gtk_list_store_clear(list_store);

      for (int ii = 0; device_list[ii]; ++ii)
      {
        #ifdef DEBUGLOG
        g_print (_("device `%s' is a %s scanner from %s of type %s\n"),
                  device_list[ii]->name, device_list[ii]->model,
                  device_list[ii]->vendor, device_list[ii]->type);
        #endif
        GtkTreeIter iter;
        gtk_list_store_append (list_store, &iter);
        gtk_list_store_set (list_store, &iter,
                            LIST_ADDRESS,    device_list[ii]->name,
                            LIST_VENDOR,    device_list[ii]->vendor,
                            LIST_MODEL,    device_list[ii]->model,
                            LIST_INFO,    device_list[ii]->type,
                            -1);
      }
  }

  sane_exit();
}

void image_scanner_fetch_attribs(const gchar * device_name)
{
  const SANE_Option_Descriptor * opt0;
  SANE_Status status;
  SANE_Handle devhandle;
  SANE_Int num_dev_options;

  sane_init (0, 0);

  status = sane_open(device_name, &devhandle);
  if (status != SANE_STATUS_GOOD) {
      g_print (_("Cannot find a scanner device, make sure it is turned on and connected to the computer.\n"));
      gimp_message (_("Cannot find a scanner device, make sure it is turned on and connected to the computer.\n"));
      sane_exit();
      return;
  }

  opt0 = sane_get_option_descriptor (devhandle, 0);
  if (opt0 == NULL) {
      g_print (_("Could not get option descriptor for option 0\n"));
      gimp_message (_("Could not get option descriptor for option 0\n"));
      sane_exit();
      return;
  }

  status = sane_control_option (devhandle, 0, SANE_ACTION_GET_VALUE, &num_dev_options, 0);
  if (status != SANE_STATUS_GOOD) {
      g_print (_("Could not get value for option 0: %s\n"), sane_strstatus (status));
      sane_exit();
      return;
  }

  for (int i = 1; i < num_dev_options; ++i)
  {
      const SANE_Option_Descriptor * opt;

      opt = sane_get_option_descriptor (devhandle, i);
      if (opt == NULL) {
          g_print (_("Could not get option descriptor for option %d\n"), i);
      }

      if (!SANE_OPTION_IS_SETTABLE (opt->cap)) {
          continue;
      }

      if (strcmp ("resolution", opt->name) == 0)
      {
        res_opt = i;
        SANE_Word const* values = opt->constraint.word_list;
        const int res_count = values[0];
        int i;
        gtk_list_store_clear(res_store1);

        GtkTreeIter iter;
        const gchar resvaluestr[256];
        for(i=1; i <= res_count; i++) {
            resolutions[i-1] = values[i];
            sprintf(resvaluestr, "%d", resolutions[i-1]);
            gtk_list_store_append (res_store1, &iter);
            gtk_list_store_set (res_store1, &iter, 0, resvaluestr, -1);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX (rescombo1), 0);
      }

      if (strcmp ("mode", opt->name) == 0)
      {
        mode_opt = i;
        gtk_list_store_clear(res_store2);
        GtkTreeIter iter;
        SANE_String_Const const* strings = opt->constraint.string_list;
        gint i = 0;
        while(*strings) {
            sprintf(modes[i], "%s\0", *strings);
            //strcpy(modes[i], *strings);
            gtk_list_store_append (res_store2, &iter);
            gtk_list_store_set (res_store2, &iter, 0, modes[i], -1);
            strings++;
            i++;
        }
        modes_count = i;
        gtk_combo_box_set_active(GTK_COMBO_BOX (rescombo2), 0);
      }

      if (strcmp ("source", opt->name) == 0)
      {
        source_opt = i;
        gtk_list_store_clear(res_store3);
        GtkTreeIter iter;
        SANE_String_Const const* strings = opt->constraint.string_list;
        gint i = 0;
        while(*strings) {
            sprintf(sources[i], "%s\0", *strings);
            //strcpy(sources[i], *strings);
            gtk_list_store_append (res_store3, &iter);
            gtk_list_store_set (res_store3, &iter, 0, sources[i], -1);
            strings++;
            i++;
        }
        source_count = i;
        gtk_combo_box_set_active(GTK_COMBO_BOX (rescombo3), 0);
      }

      if (strcmp ("tl-x", opt->name) == 0)
      {
          page_left_opt = i;
          page_left = SANE_UNFIX (opt->constraint.range->max);
          gtk_range_set_range (GTK_RANGE (crop_left_scaler), 0.0, page_left);
          gtk_range_set_value (GTK_RANGE (crop_left_scaler), 0.0);
      }

      if (strcmp ("tl-y", opt->name) == 0)
      {
          page_top_opt = i;
          page_top = SANE_UNFIX (opt->constraint.range->max);
          gtk_range_set_range (GTK_RANGE (crop_top_scaler), 0.0, page_top);
          gtk_range_set_value (GTK_RANGE (crop_top_scaler), 0.0);
      }

      if (strcmp ("br-x", opt->name) == 0)
      {
          page_right_opt = i;
          page_right = SANE_UNFIX (opt->constraint.range->max);
          gtk_range_set_range (GTK_RANGE (crop_right_scaler), 0.0, page_right);
          gtk_range_set_value (GTK_RANGE (crop_right_scaler), page_right);
      }

      if (strcmp ("br-y", opt->name) == 0)
      {
          page_bottom_opt = i;
          page_bottom = SANE_UNFIX (opt->constraint.range->max);
          gtk_range_set_range (GTK_RANGE (crop_bottom_scaler), 0.0, page_bottom);
          gtk_range_set_value (GTK_RANGE (crop_bottom_scaler), letter_h);
      }

  }

  init = FALSE;

  sane_exit();
}

static GList *
image_scanner_query_procedures (GimpPlugIn *plug_in)
{
  return g_list_append (NULL, g_strdup (PLUG_IN_PROC));
}

static void
image_scanner_scan_callback (GtkWidget *dialog)
{
  if (current_device_name == NULL)
  {
    g_print (_("ERROR no devices were selected cannot scan"));
    gimp_message (_("ERROR no devices were selected cannot scan"));
    return;
  }
  #ifdef DEBUGLOG
  g_print (_("Starting Scanning\n"));
  g_print (_("Current Device: %s\n"), current_device_name);
  #endif

  gtk_label_set_text (GTK_LABEL (message), _("Scanning document."));
  gtk_widget_show_now (message);
  gimp_progress_init (_("Scanning document."));

  flatbed_start_scan (current_device_name);

  gtk_label_set_text (GTK_LABEL (message), "");
  gimp_progress_end (); // clear progress bar message
}

static void
image_scanner_devices_callback (GtkWidget *dialog)
{
  gtk_label_set_text (GTK_LABEL (message), _("Searching for scanner devices."));
  gtk_widget_show_now (message);
  gimp_progress_init (_("Searching for scanner devices."));

  image_scanner_query_sane_for_devices ();

  gtk_label_set_text (GTK_LABEL (message), "");
  gimp_progress_end (); // clear message
}


static GtkWidget *
image_scanner_create_page_grid (GtkWidget   *notebook,
                                  const gchar *tab_name)
{
  GtkWidget *scrolled_win;
  GtkWidget *viewport;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *grid;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (box);

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 6);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (box), scrolled_win, TRUE, TRUE, 0);
  gtk_widget_show (scrolled_win);

  label = gtk_label_new (tab_name);
  gtk_widget_set_margin_start (label, 2);
  gtk_widget_set_margin_top (label, 2);
  gtk_widget_set_margin_end (label, 2);
  gtk_widget_set_margin_bottom (label, 2);
  gtk_widget_set_can_focus (label, FALSE);
  gtk_widget_show (label);

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), box, label);

  viewport = gtk_viewport_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_win), viewport);
  gtk_widget_show (viewport);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (viewport), box);
  gtk_widget_show (box);

  grid = gtk_grid_new ();
  gtk_widget_set_margin_bottom (grid, 5);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 3);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 30);
  gtk_box_pack_start (GTK_BOX (box), grid, FALSE, TRUE, 0);
  gtk_widget_show (grid);

  return grid;
}

static void
activate_scanner_callback (GtkTreeSelection *selection,
                     gpointer   data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    if (current_device_name != NULL) g_free(current_device_name);
    gtk_tree_model_get (model, &iter, 0, &current_device_name, -1);
    image_scanner_fetch_attribs(current_device_name);
    #ifdef DEBUGLOG
    g_print (_("Selected Device: %s\n"), current_device_name);
    #endif
  }
}

void
gimp_radio_button_update_wrapper (GtkWidget *widget,
                                  gpointer   data)
{
  gimp_radio_button_update (widget, data);
  g_object_set (globalConfig, "input-type", input_type, NULL);
}

void
unitscombo_callback (GtkWidget *widget,
                      gpointer   data)
{
  const gchar str[256];
  gint temp = units_measurement;
  gimp_int_combo_box_get_active(GIMP_INT_COMBO_BOX (data), &units_measurement);
  g_object_set (globalConfig, "units-measurement", units_measurement, NULL);

  left_current = gtk_range_get_value (GTK_RANGE (crop_left_scaler));
  right_current = gtk_range_get_value (GTK_RANGE (crop_right_scaler));
  top_current = gtk_range_get_value (GTK_RANGE (crop_top_scaler));
  bottom_current = gtk_range_get_value (GTK_RANGE (crop_bottom_scaler));

  if (units_measurement == IMAGE_SCANNER_MM_UNIT)
    {
      sprintf(str, "%.2f", left_current);
      gtk_label_set_label (GTK_LABEL (crop_left_scaler_label), str);

      sprintf(str, "%.2f", right_current);
      gtk_label_set_label (GTK_LABEL (crop_right_scaler_label), str);

      sprintf(str, "%.2f", top_current);
      gtk_label_set_label (GTK_LABEL (crop_top_scaler_label), str);

      sprintf(str, "%.2f", bottom_current);
      gtk_label_set_label (GTK_LABEL (crop_bottom_scaler_label), str);
    }

  if (units_measurement == IMAGE_SCANNER_CM_UNIT)
    {
      left_current /= 10;
      sprintf(str, "%.2f", left_current);
      gtk_label_set_label (GTK_LABEL (crop_left_scaler_label), str);

      right_current /= 10;
      sprintf(str, "%.2f", right_current);
      gtk_label_set_label (GTK_LABEL (crop_right_scaler_label), str);

      top_current /= 10;
      sprintf(str, "%.2f", top_current);
      gtk_label_set_label (GTK_LABEL (crop_top_scaler_label), str);

      bottom_current /= 10;
      sprintf(str, "%.2f", bottom_current);
      gtk_label_set_label (GTK_LABEL (crop_bottom_scaler_label), str);
    }

  if (units_measurement == IMAGE_SCANNER_IN_UNIT)
    {
      left_current /= 25.4;
      sprintf(str, "%.2f", left_current);
      gtk_label_set_label (GTK_LABEL (crop_left_scaler_label), str);

      right_current /= 25.4;
      sprintf(str, "%.2f", right_current);
      gtk_label_set_label (GTK_LABEL (crop_right_scaler_label), str);

      top_current /= 25.4;
      sprintf(str, "%.2f", top_current);
      gtk_label_set_label (GTK_LABEL (crop_top_scaler_label), str);

      bottom_current /= 25.4;
      sprintf(str, "%.2f", bottom_current);
      gtk_label_set_label (GTK_LABEL (crop_bottom_scaler_label), str);
    }
}

void
resolution_combo_callback (GtkWidget *widget,
                      gpointer   data)
{
  if (init) return;
  resolution_index = gtk_combo_box_get_active (GTK_COMBO_BOX (rescombo1));
}

void
mode_combo_callback (GtkWidget *widget,
                      gpointer   data)
{
  GtkTreeIter iter;
  const gchar item[256];

  if (init) return;
  mode_index = gtk_combo_box_get_active (GTK_COMBO_BOX (rescombo2));

  use_color = TRUE;
  if (strncasecmp(sources[source_index], "color", 5) != 0)
  {
    use_color = FALSE;
  }
}

void
source_combo_callback (GtkWidget *widget,
                      gpointer   data)
{
  GtkTreeIter       iter;
  GtkTreeModel     *model;
  GtkTreeSelection *selection;
  const gchar       item[256];

  if (init) return;
  source_index = gtk_combo_box_get_active (GTK_COMBO_BOX (rescombo3));

  use_flatbed = TRUE;
  if (strncasecmp(sources[source_index], "flatbed", 7) != 0)
  {
    use_flatbed = FALSE;
  }
}

void
crop_left_scaler_callback (GtkWidget *widget,
                           GtkWidget *label)
{
  const gchar str[256];
  left_current = gtk_range_get_value (GTK_RANGE (widget));
  if (units_measurement == IMAGE_SCANNER_CM_UNIT)
    {
      left_current /= 10;
      sprintf(str, "%.2f", left_current);
    }
  else if (units_measurement == IMAGE_SCANNER_IN_UNIT)
    {
      left_current /= 25.4;
      sprintf(str, "%.2f", left_current);
    }
  else
    {
      sprintf(str, "%.1f", left_current);
    }

  gtk_label_set_label (GTK_LABEL (label), str);
}

void
crop_right_scaler_callback (GtkWidget *widget,
                            GtkWidget *label)
{
  const gchar str[256];
  right_current = gtk_range_get_value (GTK_RANGE (widget));
  if (units_measurement == IMAGE_SCANNER_CM_UNIT)
    {
      right_current /= 10;
      sprintf(str, "%.2f", right_current);
    }
  else if (units_measurement == IMAGE_SCANNER_IN_UNIT)
    {
      right_current /= 25.4;
      sprintf(str, "%.2f", right_current);
    }
  else
    {
      sprintf(str, "%.1f", right_current);
    }

  gtk_label_set_label (GTK_LABEL (label), str);
}

void
crop_top_scaler_callback (GtkWidget *widget,
                          GtkWidget *label)
{
  const gchar str[256];
  top_current = gtk_range_get_value (GTK_RANGE (widget));
  if (units_measurement == IMAGE_SCANNER_CM_UNIT)
    {
      top_current /= 10;
      sprintf(str, "%.2f", top_current);
    }
  else if (units_measurement == IMAGE_SCANNER_IN_UNIT)
    {
      top_current /= 25.4;
      sprintf(str, "%.2f", top_current);
    }
  else
    {
      sprintf(str, "%.1f", top_current);
    }

  gtk_label_set_label (GTK_LABEL (label), str);
}

void
crop_bottom_scaler_callback (GtkWidget *widget,
                             GtkWidget *label)
{
  const gchar str[256];
  bottom_current = gtk_range_get_value (GTK_RANGE (widget));
  if (units_measurement == IMAGE_SCANNER_CM_UNIT)
    {
      bottom_current /= 10;
      sprintf(str, "%.2f", bottom_current);
    }
  else if (units_measurement == IMAGE_SCANNER_IN_UNIT)
    {
      bottom_current /= 25.4;
      sprintf(str, "%.2f", bottom_current);
    }
  else
    {
      sprintf(str, "%.1f", bottom_current);
    }

  gtk_label_set_label (GTK_LABEL (label), str);
}

static gboolean
image_scanner_dialog (GimpProcedure        *procedure,
                      GimpImage            *image,
                      GimpProcedureConfig  *config,
                      GError              **error)
{
  GtkWidget         *dialog;
  GtkWidget         *standardbox2;
  GtkWidget         *hbox1;
  GtkWidget         *hbox1a;
  GtkWidget         *hbox2;
  GtkWidget         *hbox2a;
  GtkWidget         *hbox3;
  GtkWidget         *hbox3a;
  GtkWidget         *hbox4;
  GtkWidget         *hbox4a;
  GtkWidget         *hbox5;
  GtkWidget         *crop_left_label;
  GtkWidget         *crop_right_label;
  GtkWidget         *crop_top_label;
  GtkWidget         *crop_bottom_label;
  GtkWidget         *unitslabel1;
  GtkWidget         *unitscombo1;
  GtkWidget         *scanbutton;
  GtkWidget         *devicesbutton;
  GtkWidget         *content_area;
  GtkWidget         *image_scanner_vbox;
  GtkWidget         *image_scanner_hbox;
  GtkWidget         *notebook;
  GtkWidget         *spinner;
  GtkWidget         *grid1;
  GtkWidget         *grid2;
  GtkWidget         *grid3;
  GtkWidget         *grid4;
  GtkWidget         *radiobox;
  GtkWidget         *gimpradiogroup;
  GtkWidget         *standardbox;
  GtkWidget         *resbox1;
  GtkWidget         *reslabel1;
  GtkWidget         *resbox2;
  GtkWidget         *reslabel2;
  GtkWidget         *resbox3;
  GtkWidget         *reslabel3;
  GtkTreeIter        iter;
  GtkCellRenderer   *rescell1;
  GtkCellRenderer   *rescell2;
  GtkCellRenderer   *rescell3;
  GtkTreeSelection  *select;

  g_object_get (config,
                "input-type", &input_type,
                "units-measurement", &units_measurement,
                "resolution-index", &resolution_index,
                "mode-index", &mode_index,
                "source-index", &source_index,
                NULL);

  dialog = gimp_dialog_new (_("Image Scanner (SANE)"),
                          PLUG_IN_ROLE,
                          NULL,
                          0,
                          NULL,
                          PLUG_IN_PROC,
                          _("Close"),  GTK_RESPONSE_CLOSE,
                          NULL);

  gtk_widget_set_size_request(dialog, 650, 500);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  image_scanner_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (content_area), image_scanner_vbox, TRUE, TRUE, 0);

  gtk_container_set_border_width (GTK_CONTAINER (image_scanner_vbox), 12);
  gtk_box_set_spacing (GTK_BOX (image_scanner_vbox), 6);
  gtk_widget_show (image_scanner_vbox);

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (image_scanner_vbox), notebook, TRUE, TRUE, 0);

  grid1 = image_scanner_create_page_grid (notebook, _("Devices"));
  grid2 = image_scanner_create_page_grid (notebook, _("Standard"));
  grid3 = image_scanner_create_page_grid (notebook, _("Geometry"));
  grid4 = image_scanner_create_page_grid (notebook, _("Layers"));

  gtk_container_set_border_width (GTK_CONTAINER (grid1), 0);

  gtk_widget_show (notebook);

  /* Deviecs Tab */
  list_store = gtk_list_store_new (n_scanner_device_info,
                          G_TYPE_STRING,
                          G_TYPE_STRING,
                          G_TYPE_STRING,
                          G_TYPE_STRING);
  list_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
  select = gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));
  g_signal_connect (select, "changed", G_CALLBACK (activate_scanner_callback), list_view);
  gtk_container_add (GTK_CONTAINER (grid1), list_view);
  gtk_widget_show (list_view);

  g_object_unref (list_store);

  for (int i = 0; i < 4; i++)
  {
    GtkTreeViewColumn *column;
    GtkCellRenderer   *render;
    render = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_(scanner_device_info[i].label),
                                                       render,
                                                       "text", i,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_spacing (column, 3);
    gtk_tree_view_append_column (GTK_TREE_VIEW (list_view), column);
  }

  /* Standard Tab */
  standardbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 14);
  gtk_box_set_homogeneous (GTK_BOX (standardbox), TRUE);

  /* show unset for init */
  res_store1 = gtk_list_store_new (1,
                        G_TYPE_STRING);
  gtk_list_store_append (res_store1, &iter);
  gtk_list_store_set (res_store1, &iter, 0, "*unset*", -1);
  resbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  reslabel1 = gtk_label_new (_("Scan resolution"));
  gtk_container_add (GTK_CONTAINER (resbox1), reslabel1);
  rescombo1 = gtk_combo_box_new_with_model (GTK_TREE_MODEL (res_store1));
  gtk_widget_set_size_request(rescombo1, 140, 20);
  g_object_unref (res_store1);
  rescell1 = gtk_cell_renderer_text_new ();
  g_object_set (rescell1,
                "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (rescombo1), rescell1, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (rescombo1), rescell1,
                                 "text", 0);
  gtk_combo_box_set_active(GTK_COMBO_BOX (rescombo1), 0);
  g_signal_connect (rescombo1, "changed",
                          G_CALLBACK (resolution_combo_callback),
                          GINT_TO_POINTER (0));
  gtk_box_pack_end (GTK_BOX (resbox1), rescombo1, FALSE, FALSE, 30);
  gtk_container_add (GTK_CONTAINER (standardbox), resbox1);

  gtk_container_add (GTK_CONTAINER (grid2), standardbox);
  gtk_widget_show (standardbox);
  gtk_widget_show (resbox1);
  gtk_widget_show (reslabel1);
  gtk_widget_show (rescombo1);

  res_store2 = gtk_list_store_new (1,
                        G_TYPE_STRING);
  gtk_list_store_append (res_store2, &iter);
  gtk_list_store_set (res_store2, &iter, 0, "*unset*", -1);
  resbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  reslabel2 = gtk_label_new (_("Scan mode"));
  gtk_container_add (GTK_CONTAINER (resbox2), reslabel2);
  rescombo2 = gtk_combo_box_new_with_model (GTK_TREE_MODEL (res_store2));
  gtk_widget_set_size_request(rescombo2, 140, 20);
  g_object_unref (res_store2);
  rescell2 = gtk_cell_renderer_text_new ();
  g_object_set (rescell2,
                "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (rescombo2), rescell2, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (rescombo2), rescell2,
                                 "text", 0);
  gtk_combo_box_set_active(GTK_COMBO_BOX (rescombo2), 0);
  g_signal_connect (rescombo2, "changed",
                          G_CALLBACK (mode_combo_callback),
                          GINT_TO_POINTER (0));
  gtk_box_pack_end (GTK_BOX (resbox2), rescombo2, FALSE, FALSE, 30);
  gtk_container_add (GTK_CONTAINER (standardbox), resbox2);

  gtk_widget_show (resbox2);
  gtk_widget_show (reslabel2);
  gtk_widget_show (rescombo2);

  res_store3 = gtk_list_store_new (1,
                        G_TYPE_STRING);
  gtk_list_store_append (res_store3, &iter);
  gtk_list_store_set (res_store3, &iter, 0, "*unset*", -1);
  resbox3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  reslabel3 = gtk_label_new (_("Scan source"));
  gtk_container_add (GTK_CONTAINER (resbox3), reslabel3);
  rescombo3 = gtk_combo_box_new_with_model (GTK_TREE_MODEL (res_store3));
  gtk_widget_set_size_request(rescombo3, 140, 20);
  g_object_unref (res_store3);
  rescell3 = gtk_cell_renderer_text_new ();
  g_object_set (rescell3,
                "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (rescombo3), rescell3, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (rescombo3), rescell3,
                                 "text", 0);
  gtk_combo_box_set_active(GTK_COMBO_BOX (rescombo3), 0);
  g_signal_connect (rescombo3, "changed",
                          G_CALLBACK (source_combo_callback),
                          GINT_TO_POINTER (0));
  gtk_box_pack_end (GTK_BOX (resbox3), rescombo3, FALSE, FALSE, 30);
  gtk_container_add (GTK_CONTAINER (standardbox), resbox3);

  gtk_widget_show (resbox3);
  gtk_widget_show (reslabel3);
  gtk_widget_show (rescombo3);

  /* Geometry Tab */
  standardbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 14);
  gtk_box_set_homogeneous (GTK_BOX (standardbox), TRUE);

  int digits = 1;
  if (units_measurement != IMAGE_SCANNER_MM_UNIT) digits = 2;

  // =============
  hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  crop_left_label = gtk_label_new (_("Page crop left"));
  gtk_container_add (GTK_CONTAINER (hbox1), crop_left_label);

  hbox1a = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  crop_left_scaler_label = gtk_label_new ("0.0");
  gtk_widget_set_size_request(crop_left_scaler_label, 40, 10);
  gtk_box_pack_end (GTK_BOX (hbox1a), crop_left_scaler_label, FALSE, FALSE, 3);

  crop_left_scaler = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, page_left, 1.0);
  gtk_scale_set_digits(GTK_SCALE (crop_left_scaler), digits);
  gtk_scale_set_has_origin(GTK_SCALE (crop_left_scaler), TRUE);
  gtk_scale_set_draw_value(GTK_SCALE (crop_left_scaler), FALSE);
  gtk_scale_set_value_pos(GTK_SCALE (crop_left_scaler), GTK_POS_RIGHT);
  gtk_widget_set_size_request(crop_left_scaler, 300, 10);
  gtk_box_pack_end (GTK_BOX (hbox1a), crop_left_scaler, FALSE, FALSE, 3);
  gtk_box_pack_end (GTK_BOX (hbox1), hbox1a, FALSE, FALSE, 40);
  gtk_container_add (GTK_CONTAINER (standardbox2), hbox1);
  g_signal_connect (crop_left_scaler,
                    "value-changed",
                    G_CALLBACK(crop_left_scaler_callback),
                    crop_left_scaler_label);

  // =============
  hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  crop_top_label = gtk_label_new (_("Page crop top"));
  gtk_container_add (GTK_CONTAINER (hbox2), crop_top_label);

  hbox2a = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  crop_top_scaler_label = gtk_label_new ("0.0");
  gtk_widget_set_size_request(crop_top_scaler_label, 40, 10);
  gtk_box_pack_end (GTK_BOX (hbox2a), crop_top_scaler_label, FALSE, FALSE, 3);

  crop_top_scaler = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, page_top, 1.0);
  gtk_scale_set_digits(GTK_SCALE (crop_top_scaler), digits);
  gtk_scale_set_has_origin(GTK_SCALE (crop_top_scaler), TRUE);
  gtk_scale_set_draw_value(GTK_SCALE (crop_top_scaler), FALSE);
  gtk_scale_set_value_pos(GTK_SCALE (crop_top_scaler), GTK_POS_RIGHT);
  gtk_widget_set_size_request(crop_top_scaler, 300, 10);
  gtk_box_pack_end (GTK_BOX (hbox2a), crop_top_scaler, FALSE, FALSE, 3);
  gtk_box_pack_end (GTK_BOX (hbox2), hbox2a, FALSE, FALSE, 40);
  gtk_container_add (GTK_CONTAINER (standardbox2), hbox2);
  g_signal_connect (crop_top_scaler,
                    "value-changed",
                    G_CALLBACK(crop_top_scaler_callback),
                    crop_top_scaler_label);

  // =============
  hbox3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  crop_right_label = gtk_label_new (_("Page crop right"));
  gtk_container_add (GTK_CONTAINER (hbox3), crop_right_label);

  hbox3a = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  crop_right_scaler_label = gtk_label_new ("0.0");
  gtk_widget_set_size_request(crop_right_scaler_label, 40, 10);
  gtk_box_pack_end (GTK_BOX (hbox3a), crop_right_scaler_label, FALSE, FALSE, 3);

  crop_right_scaler = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, page_right, 1.0);
  gtk_scale_set_digits(GTK_SCALE (crop_right_scaler), digits);
  gtk_scale_set_has_origin(GTK_SCALE (crop_right_scaler), TRUE);
  gtk_scale_set_draw_value(GTK_SCALE (crop_right_scaler), FALSE);
  gtk_scale_set_value_pos(GTK_SCALE (crop_right_scaler), GTK_POS_RIGHT);
  gtk_widget_set_size_request(crop_right_scaler, 300, 10);
  gtk_box_pack_end (GTK_BOX (hbox3a), crop_right_scaler, FALSE, FALSE, 3);
  gtk_box_pack_end (GTK_BOX (hbox3), hbox3a, FALSE, FALSE, 40);
  gtk_container_add (GTK_CONTAINER (standardbox2), hbox3);
  g_signal_connect (crop_right_scaler,
                    "value-changed",
                    G_CALLBACK(crop_right_scaler_callback),
                    crop_right_scaler_label);

  // =============
  hbox4 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  crop_bottom_label = gtk_label_new (_("Page crop bottom"));
  gtk_container_add (GTK_CONTAINER (hbox4), crop_bottom_label);

  hbox4a = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  crop_bottom_scaler_label = gtk_label_new ("0.0");
  gtk_widget_set_size_request(crop_bottom_scaler_label, 40, 10);
  gtk_box_pack_end (GTK_BOX (hbox4a), crop_bottom_scaler_label, FALSE, FALSE, 3);

  crop_bottom_scaler = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, page_bottom, 1.0);
  gtk_scale_set_digits(GTK_SCALE (crop_bottom_scaler), digits);
  gtk_scale_set_has_origin(GTK_SCALE (crop_bottom_scaler), TRUE);
  gtk_scale_set_draw_value(GTK_SCALE (crop_bottom_scaler), FALSE);
  gtk_scale_set_value_pos(GTK_SCALE (crop_bottom_scaler), GTK_POS_RIGHT);
  gtk_widget_set_size_request(crop_bottom_scaler, 300, 10);
  gtk_box_pack_end (GTK_BOX (hbox4a), crop_bottom_scaler, FALSE, FALSE, 3);
  gtk_box_pack_end (GTK_BOX (hbox4), hbox4a, FALSE, FALSE, 40);
  gtk_container_add (GTK_CONTAINER (standardbox2), hbox4);
  g_signal_connect (crop_bottom_scaler,
                    "value-changed",
                    G_CALLBACK(crop_bottom_scaler_callback),
                    crop_bottom_scaler_label);

  // =============
  hbox5 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  unitslabel1 = gtk_label_new (_("Show units as"));
  gtk_container_add (GTK_CONTAINER (hbox5), unitslabel1);
  unitscombo1 = gimp_int_combo_box_new(
          _("  mm  "), IMAGE_SCANNER_MM_UNIT,
          _("  cm  "), IMAGE_SCANNER_CM_UNIT,
          _("  in  "), IMAGE_SCANNER_IN_UNIT,
           NULL);
  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (unitscombo1),
                               units_measurement);
  g_signal_connect (unitscombo1, "changed",
                          G_CALLBACK (unitscombo_callback),
                          unitscombo1);
  gtk_box_pack_start (GTK_BOX (hbox5), unitscombo1, FALSE, FALSE, 30);
  gtk_container_add (GTK_CONTAINER (standardbox2), hbox5);

  gtk_container_add (GTK_CONTAINER (grid3), standardbox2);
  gtk_widget_show (standardbox2);
  gtk_widget_show (hbox1);
  gtk_widget_show (hbox1a);
  gtk_widget_show (hbox2);
  gtk_widget_show (hbox2a);
  gtk_widget_show (hbox3);
  gtk_widget_show (hbox3a);
  gtk_widget_show (hbox4);
  gtk_widget_show (hbox4a);
  gtk_widget_show (hbox5);

  gtk_widget_show (crop_left_label);
  gtk_widget_show (crop_left_scaler);
  gtk_widget_show (crop_left_scaler_label);
  gtk_widget_show (crop_right_label);
  gtk_widget_show (crop_right_scaler);
  gtk_widget_show (crop_right_scaler_label);
  gtk_widget_show (crop_top_label);
  gtk_widget_show (crop_top_scaler);
  gtk_widget_show (crop_top_scaler_label);
  gtk_widget_show (crop_bottom_label);
  gtk_widget_show (crop_bottom_scaler);
  gtk_widget_show (crop_bottom_scaler_label);

  gtk_widget_show (unitslabel1);
  gtk_widget_show (unitscombo1);

  /* Layers Tab */
  radiobox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_set_homogeneous (GTK_BOX (radiobox), TRUE);
  gimpradiogroup = gimp_int_radio_group_new (TRUE,
                                      _("Select ADF image input method :"),
                                      G_CALLBACK (gimp_radio_button_update_wrapper),
                                      &input_type, NULL, input_type,
                                      _("Add as new layer"),                 IMAGE_SCANNER_CURRENT_LAYER, NULL,
                                      _("Add as new images only"),           IMAGE_SCANNER_NEW_IMAGE,     NULL,
                                      NULL);
  gtk_box_pack_start (GTK_BOX (radiobox), gimpradiogroup, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (grid4), radiobox);
  gtk_widget_show (radiobox);
  gtk_widget_show (gimpradiogroup);


  /* Buttons and spinner for bottom */
  image_scanner_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_size_request(image_scanner_hbox, 650, 60);
  gtk_box_pack_start (GTK_BOX (content_area), image_scanner_hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (image_scanner_hbox), 12);
  gtk_box_set_spacing (GTK_BOX (image_scanner_hbox), 6);

  message = gtk_label_new ("");
  gtk_label_set_single_line_mode(GTK_LABEL (message), TRUE);
  gtk_label_set_justify (GTK_LABEL (message), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (image_scanner_hbox), message, FALSE, FALSE, 6);
  gtk_widget_show (message);

  #if  0
  spinner = gtk_spinner_new ();
  gtk_widget_set_size_request (spinner, 30, 30);
  gtk_box_pack_start (GTK_BOX (image_scanner_hbox), spinner, FALSE, FALSE, 0);
  gtk_widget_show (spinner);
  gtk_spinner_start (GTK_SPINNER (spinner));
  #endif

  #if 0
  progressbar = gtk_progress_bar_new();
  gtk_widget_set_size_request (progressbar, 300, 30);
  gtk_box_pack_start (GTK_BOX (image_scanner_hbox), progressbar, FALSE, TRUE, 0);
  gtk_widget_show (progressbar);
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (progressbar), TRUE);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar), "TEST");
  gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR(progressbar), 0.5);
  gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));
  #endif

  scanbutton = gimp_button_new ();
  gtk_widget_set_size_request (scanbutton, 110, 30);
  gtk_button_set_relief (GTK_BUTTON (scanbutton), FALSE);
  gtk_button_set_label (GTK_BUTTON (scanbutton), _("Scan"));
  gtk_box_pack_end (GTK_BOX (image_scanner_hbox), scanbutton, FALSE, FALSE, 0);
  gtk_widget_show (scanbutton);

  g_signal_connect_object (scanbutton,
                           "clicked",
                           G_CALLBACK (image_scanner_scan_callback),
                           dialog,
                           0);

  devicesbutton = gimp_button_new ();
  gtk_widget_set_size_request (devicesbutton, 110, 30);
  gtk_button_set_relief (GTK_BUTTON (devicesbutton), FALSE);
  gtk_button_set_label (GTK_BUTTON (devicesbutton), _("Find Devices"));
  gtk_box_pack_end (GTK_BOX (image_scanner_hbox), devicesbutton, FALSE, FALSE, 0);
  gtk_widget_show (devicesbutton);

  g_signal_connect_object (devicesbutton,
                           "clicked",
                           G_CALLBACK (image_scanner_devices_callback),
                           dialog,
                           0);

  gtk_widget_show (image_scanner_hbox);

  gimp_dialog_run (GIMP_DIALOG (dialog));

  return TRUE;
}

static GimpValueArray *
image_scanner_run (GimpProcedure *procedure,
                   GimpRunMode run_mode,
                   GimpImage *image,
                   GimpDrawable **drawables,
                   GimpProcedureConfig *config,
                   gpointer run_data)
{
  GError       *error  = NULL;

  gimp_ui_init (PLUG_IN_BINARY);

  globalConfig = config;

  if (image_scanner_dialog (procedure, image, config, &error))
  {
    g_object_set (globalConfig, "input-type", input_type, NULL);
    return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
  }
  else
  {
    return gimp_procedure_new_return_values (procedure, GIMP_PDB_EXECUTION_ERROR, error);
  }
}

static GimpProcedure *
image_scanner_create_procedure (GimpPlugIn *plug_in, const gchar *name)
{
  GimpProcedure *procedure = NULL;

  if (g_strcmp0 (name, PLUG_IN_PROC) == 0)
  {
    procedure = gimp_image_procedure_new (plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN,
                                  image_scanner_run, NULL, NULL);

    gimp_procedure_set_image_types (procedure, "*");
    gimp_procedure_set_sensitivity_mask (procedure, GIMP_PROCEDURE_SENSITIVE_ALWAYS);

    gimp_procedure_set_icon_name (procedure, GIMP_ICON_INPUT_DEVICE);
    gimp_procedure_set_menu_label (procedure, _("_Image Scanner (SANE)"));
    gimp_procedure_add_menu_path (procedure, _("<Image>/File/Create"));

    gimp_procedure_set_documentation (procedure,
                                      _("Image Scanner (SANE)"),
                                      _("Image Scanner plugin for GIMP to import images from SANE devices"),
                                      NULL);

    gimp_procedure_set_attribution (procedure, "Draekko", "Draekko", "2025");

    gimp_procedure_add_int_argument (procedure, "input-type",
                                     _("Image Input Type"), NULL,
                                     IMAGE_SCANNER_CURRENT_LAYER,
                                     IMAGE_SCANNER_NEW_IMAGE,
                                     IMAGE_SCANNER_CURRENT_LAYER,
                                     G_PARAM_READWRITE);

    gimp_procedure_add_int_argument (procedure, "units-measurement",
                                     _("Image Unit Measurement Type"), NULL,
                                     IMAGE_SCANNER_MM_UNIT,
                                     IMAGE_SCANNER_IN_UNIT,
                                     IMAGE_SCANNER_IN_UNIT,
                                     G_PARAM_READWRITE);

    gimp_procedure_add_int_argument (procedure, "resolution-index",
                                     _("Resolution Index"), NULL,
                                     0,
                                     100,
                                     0,
                                     G_PARAM_READWRITE);

    gimp_procedure_add_int_argument (procedure, "mode-index",
                                     _("Mode Index"), NULL,
                                     0,
                                     100,
                                     0,
                                     G_PARAM_READWRITE);

    gimp_procedure_add_int_argument (procedure, "source-index",
                                     _("Source Index"), NULL,
                                     0,
                                     100,
                                     0,
                                     G_PARAM_READWRITE);
  }

  return procedure;
}

static void
imagescanner_class_init (ImageScannerClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);
  plug_in_class->query_procedures = image_scanner_query_procedures;
  plug_in_class->create_procedure = image_scanner_create_procedure;
}
