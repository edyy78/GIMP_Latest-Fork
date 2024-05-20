/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpglyphspanel.c
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

#include <gtk/gtk.h>

#include "widgets-types.h"

#include "libgimpwidgets/gimpwidgets.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"

#include "text/gimpfont.h"

#include "gimpcontainerentry.h"
#include "gimpcontainerview.h"
#include "gimpcontainercombobox.h"
#include "gimpglyphspanel.h"

#include "gimp-intl.h"

#define COLUMNS      4
#define CELL_SIZE    50
#define EMPTY_SET_TEXT_SIZE PANGO_SCALE * 15

static void      gimp_glyphs_panel_finalize             (GObject         *object);
static void      gimp_glyphs_panel_set_property         (GObject         *object,
                                                         guint            property_id,
                                                         const GValue    *value,
                                                         GParamSpec      *pspec);
static void     gimp_glyphs_panel_constructed           (GObject         *object);
static gboolean gimp_glyphs_panel_draw                  (GtkWidget        *widget,
                                                         cairo_t          *cr,
                                                         GimpGlyphsPanel  *panel);
static void     gimp_glyphs_panel_set_alternates_sets   (GimpGlyphsPanel  *panel);
static gboolean gimp_glyphs_panel_font_changed          (GimpContext      *context,
                                                         GimpFont         *font,
                                                         GimpGlyphsPanel  *panel);
static gboolean gimp_glyphs_panel_stylistic_set_changed (GtkComboBox      *widget,
                                                         GimpGlyphsPanel  *panel);
static gboolean gimp_glyphs_panel_button_press_event    (GtkWidget        *widget,
                                                         GdkEvent         *event,
                                                         GimpGlyphsPanel  *panel);
static gboolean gimp_glyphs_panel_key_press_event       (GtkWidget        *widget,
                                                         GdkEvent         *event,
                                                         GimpGlyphsPanel  *panel);
static gboolean gimp_glyphs_panel_search                (GtkEntry         *widget,
                                                         GimpGlyphsPanel  *panel);

enum
{
  PROP_0,
  PROP_GIMP_CONTEXT,
  N_PROPS
};

struct _GimpGlyphsPanel
{
  GimpDataEditor  parent_instance;

  GimpContext    *context;
  GimpContext    *user_context;
  GimpContainer  *fonts;
  GtkWidget      *font_entry;
  GtkWidget      *stylistic_set_entry;
  GtkWidget      *palette_window;
  GtkWidget      *palette;
  GtkWidget      *search;
  GList          *current_glyphs;
  guint           selected_glyph;
  guint           glyphs_count;
};

struct _GimpGlyphsPanelClass
{
  GimpDataEditorClass  parent_class;
};

G_DEFINE_TYPE (GimpGlyphsPanel, gimp_glyphs_panel, GIMP_TYPE_EDITOR)

#define parent_class gimp_glyphs_panel_parent_class

static void
gimp_glyphs_panel_class_init (GimpGlyphsPanelClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = gimp_glyphs_panel_constructed;
  object_class->set_property = gimp_glyphs_panel_set_property;
  object_class->finalize     = gimp_glyphs_panel_finalize;

  g_object_class_install_property (object_class, PROP_GIMP_CONTEXT,
                                   g_param_spec_object ("user-context",
                                                        NULL, NULL,
                                                        GIMP_TYPE_CONTEXT,
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_WRITABLE       |
                                                        G_PARAM_CONSTRUCT_ONLY));

}

static void
gimp_glyphs_panel_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GimpGlyphsPanel *panel =  GIMP_GLYPHS_PANEL(object);

  switch (property_id)
  {
    case PROP_GIMP_CONTEXT:
      panel->user_context = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gimp_glyphs_panel_init (GimpGlyphsPanel *panel)
{
}

static void
gimp_glyphs_panel_constructed (GObject *object)
{
  GimpGlyphsPanel *panel = GIMP_GLYPHS_PANEL (object);

  G_OBJECT_CLASS (parent_class)->constructed (object);

  gtk_box_set_spacing (GTK_BOX (panel), 2);

  panel->font_entry = gimp_container_entry_new (NULL, NULL, GIMP_VIEW_SIZE_SMALL, 1);

  panel->stylistic_set_entry = gtk_combo_box_text_new ();

  panel->search = gtk_entry_new ();

  panel->palette_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_size_request (panel->palette_window , -1, -1);
  gtk_widget_set_margin_start (panel->palette_window, 5);
  gtk_widget_set_margin_end (panel->palette_window, 5);
  gtk_widget_set_margin_top (panel->palette_window, 5);
  gtk_widget_set_margin_bottom (panel->palette_window, 5);

  panel->palette = gtk_drawing_area_new ();
  gtk_widget_set_halign (panel->palette, GTK_ALIGN_CENTER);

  gimp_help_set_help_data (panel->font_entry,
                           _("Change font"), NULL);

  gimp_help_set_help_data (panel->stylistic_set_entry,
                           _("Change stylistic set"), NULL);

  gimp_help_set_help_data (panel->search,
                           _("Find alternates of a specific string"), NULL);

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (panel->stylistic_set_entry), "default glyphs");
  gtk_combo_box_set_active (GTK_COMBO_BOX (panel->stylistic_set_entry), 0);
  gtk_container_add (GTK_CONTAINER (panel), panel->font_entry);
  gtk_container_add (GTK_CONTAINER (panel), panel->stylistic_set_entry);
  gtk_container_add (GTK_CONTAINER (panel), panel->search);
  gtk_container_add (GTK_CONTAINER (panel->palette_window), panel->palette);
  gtk_box_pack_start (GTK_BOX (panel), panel->palette_window, TRUE, TRUE, 0);

  panel->context = gimp_context_new (panel->user_context->gimp,
                                      "glyphs-palette-context",
                                      panel->user_context);

  panel->selected_glyph = -1;

  panel->fonts = gimp_data_factory_get_container (panel->user_context->gimp->font_factory);

  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (panel->font_entry),
                                   panel->context);

  gimp_container_view_set_container (GIMP_CONTAINER_VIEW (panel->font_entry),
                                     panel->fonts);

  gtk_widget_add_events (GTK_WIDGET (panel->palette), GDK_BUTTON_PRESS_MASK);

  g_signal_connect (G_OBJECT (panel->palette), "button-press-event",
                    G_CALLBACK (gimp_glyphs_panel_button_press_event), panel);

  g_signal_connect (G_OBJECT (panel->palette), "draw",
                    G_CALLBACK (gimp_glyphs_panel_draw), panel);

  g_signal_connect (G_OBJECT (panel->stylistic_set_entry), "changed",
                    G_CALLBACK (gimp_glyphs_panel_stylistic_set_changed), panel);

  g_signal_connect (G_OBJECT (panel->palette_window), "key-press-event",
                    G_CALLBACK (gimp_glyphs_panel_key_press_event), panel);

  g_signal_connect (G_OBJECT (panel->search), "activate",
                    G_CALLBACK (gimp_glyphs_panel_search), panel);

  g_signal_connect (G_OBJECT (panel->context), "font-changed",
                    G_CALLBACK (gimp_glyphs_panel_font_changed), panel);

  gimp_glyphs_panel_set_alternates_sets (panel);

  gtk_widget_show (panel->font_entry);
  gtk_widget_show (panel->stylistic_set_entry);
  gtk_widget_show (panel->search);
  gtk_widget_show (panel->palette_window);
  gtk_widget_show (panel->palette);
}

static void
gimp_glyphs_panel_set_alternates_sets (GimpGlyphsPanel *panel)
{
  GtkListStore *list_store      = gtk_list_store_new (1, G_TYPE_STRING);
  GList        *sets            = gimp_font_get_alternates_sets (gimp_context_get_font (panel->context));
  GtkComboBox  *alternates_sets = GTK_COMBO_BOX (panel->stylistic_set_entry);

  gtk_list_store_insert_with_values (list_store, NULL, -1, 0, "default glyphs", -1);

  if (sets)
    for (;(sets = sets->next);)
      gtk_list_store_insert_with_values (list_store, NULL, -1, 0, (gchar*)sets->data, -1);

  gtk_combo_box_set_model (alternates_sets, GTK_TREE_MODEL (list_store));
  gtk_combo_box_set_active (alternates_sets, 0);
  panel->selected_glyph = -1;

  g_object_unref (list_store);
}

static gboolean
gimp_glyphs_panel_font_changed (GimpContext     *context,
                                GimpFont        *font,
                                GimpGlyphsPanel *panel)
{
  gimp_glyphs_panel_set_alternates_sets (panel);
  gtk_widget_queue_draw (panel->palette);

   return TRUE;
}

static void
gimp_glyphs_panel_finalize (GObject *object)
{
  GimpGlyphsPanel *panel = GIMP_GLYPHS_PANEL (object);

  g_signal_handlers_disconnect_by_func (G_OBJECT (panel->context),
                                        G_CALLBACK (gimp_glyphs_panel_font_changed), panel);
  g_clear_object (&panel->context);

  g_list_free_full (panel->current_glyphs, (GDestroyNotify)g_free);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gimp_glyphs_panel_button_press_event  (GtkWidget       *widget,
                                       GdkEvent        *event,
                                       GimpGlyphsPanel *panel)
{
  gdouble  y;
  gdouble  x;
  guint    row;
  guint    col;
  guint    entry_pos;
  GList   *list;

  if (((GdkEventButton*)event)->button != GDK_BUTTON_PRIMARY)
    return TRUE;

  y         = ((GdkEventButton*)event)->x;
  x         = ((GdkEventButton*)event)->y;
  row       = x/CELL_SIZE;
  col       = y/CELL_SIZE;
  entry_pos = row*COLUMNS+col;
  list      = panel->current_glyphs;

  for (guint i = 0; list && i < entry_pos; i++)
    list = list->next;

  if (entry_pos < panel->glyphs_count)
    {
       panel->selected_glyph = entry_pos;
       gtk_widget_queue_draw (panel->palette);
       g_signal_emit_by_name (panel->user_context, "glyph-changed", (gchar*)(list->data));
    }

  gtk_widget_grab_focus (panel->palette_window);

  return TRUE;
}

static gboolean
gimp_glyphs_panel_key_press_event  (GtkWidget       *widget,
                                    GdkEvent        *event,
                                    GimpGlyphsPanel *panel)
{
  GdkEventKey *key_event = (GdkEventKey*)event;

  switch (key_event->keyval)
    {
      case GDK_KEY_Left:
        if (panel->selected_glyph > 0)
          {
            panel->selected_glyph--;
            gtk_widget_queue_draw (panel->palette);
          }
        return TRUE;
      case GDK_KEY_Right:
        if (panel->selected_glyph < panel->glyphs_count-1 || panel->selected_glyph == -1)
          {
            panel->selected_glyph++;
            gtk_widget_queue_draw (panel->palette);
          }
        return TRUE;
      case GDK_KEY_Up:
        if (panel->selected_glyph >= COLUMNS)
          {
            panel->selected_glyph -= COLUMNS;
            gtk_widget_queue_draw (panel->palette);
          }
        return TRUE;
      case GDK_KEY_Down:
        if (panel->selected_glyph < panel->glyphs_count-COLUMNS)
          {
            panel->selected_glyph += COLUMNS;
            gtk_widget_queue_draw (panel->palette);
          }
        return TRUE;
      case GDK_KEY_Page_Up:
        {
          gboolean ret;
          g_signal_emit_by_name (panel->palette_window, "scroll-child", GTK_SCROLL_PAGE_UP, FALSE, &ret);
        }
        return TRUE;
      case GDK_KEY_Page_Down:
        {
          gboolean ret;
          g_signal_emit_by_name (panel->palette_window, "scroll-child", GTK_SCROLL_PAGE_DOWN, FALSE, &ret);
        }
        return TRUE;
      case GDK_KEY_Start:
        {
          gboolean ret;
          g_signal_emit_by_name (panel->palette_window, "scroll-child", GTK_SCROLL_START, FALSE, &ret);
        }
        return TRUE;
      case GDK_KEY_End:
        {
          gboolean ret;
          g_signal_emit_by_name (panel->palette_window, "scroll-child", GTK_SCROLL_END, FALSE, &ret);
        }
        return TRUE;
      case GDK_KEY_Return:
      case GDK_KEY_KP_Enter:
        if (panel->selected_glyph != -1)
          {
            GList *list = panel->current_glyphs;

            for (guint i = 0; i < panel->selected_glyph; i++)
              list = list->next;
            g_signal_emit_by_name (panel->user_context, "glyph-changed", (gchar*)(list->data));
          }
        return TRUE;
      case GDK_KEY_Num_Lock:
        return TRUE;
      default:
        break;
    }
  return FALSE;
}
static gboolean
gimp_glyphs_panel_stylistic_set_changed (GtkComboBox     *widget,
                                         GimpGlyphsPanel *panel)
{
   panel->selected_glyph = -1;
   gtk_widget_queue_draw (panel->palette);

   return TRUE;
}

static gboolean
gimp_glyphs_panel_search (GtkEntry        *widget,
                          GimpGlyphsPanel *panel)
{
  gtk_widget_queue_draw (panel->palette);
  return TRUE;
}

static gboolean
gimp_glyphs_panel_draw (GtkWidget       *widget,
                        cairo_t         *cr,
                        GimpGlyphsPanel *panel)
{
  guint            row          = 0;
  guint            col          = 0;
  PangoFontMap    *fontmap      = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
  PangoContext    *context      = pango_font_map_create_context (fontmap);
  PangoLayout     *layout       = pango_layout_new (context);
  GimpFont        *font         = gimp_context_get_font (panel->context);
  GList           *codepoints   = NULL;
  GtkEntry        *search       = GTK_ENTRY (panel->search);
  GtkComboBoxText *active_set   = GTK_COMBO_BOX_TEXT (panel->stylistic_set_entry);

  g_list_free_full (panel->current_glyphs, (GDestroyNotify)g_free);

  cairo_set_source_rgb (cr, 1, 1, 1); /* white for the background */
  cairo_paint (cr);

  if (g_strcmp0 ("default glyphs", gtk_combo_box_text_get_active_text (active_set)))
    {
      if (strlen(gtk_entry_get_text (search)) > 0)
        panel->current_glyphs = gimp_font_get_string_substitutes (font, gtk_entry_get_text (search), gtk_combo_box_text_get_active_text (active_set));
      else
        panel->current_glyphs = gimp_font_get_glyphs_in_feature (font, gtk_combo_box_text_get_active_text (active_set));
    }
  else
    {
      if (strlen(gtk_entry_get_text (search)) > 0)
        panel->current_glyphs = gimp_font_get_string_substitutes (font, gtk_entry_get_text (search), "aalt");
      else
        panel->current_glyphs = gimp_font_get_nominal_glyphs (font);
    }

  panel->glyphs_count = g_list_length (panel->current_glyphs);

  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); /* black color for text */
  codepoints = panel->current_glyphs;

  for (guint i = 0; codepoints; codepoints = codepoints->next, i++)
    {
      cairo_rectangle (cr, col*CELL_SIZE, row*CELL_SIZE, CELL_SIZE, CELL_SIZE);

      if (i == panel->selected_glyph)
        {
          cairo_set_source_rgba (cr, 0.34, 0.57, 0.85, 0.3); /* blue color for selection */
          cairo_fill_preserve (cr);
          cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
        }

      pango_layout_set_markup (layout, (gchar*)(codepoints->data), -1);
      cairo_save (cr);
      cairo_clip(cr);
      cairo_move_to (cr, col*CELL_SIZE+CELL_SIZE/3, row*CELL_SIZE+CELL_SIZE/4);
      pango_cairo_show_layout (cr, layout);

      /*bottom border*/
      cairo_move_to(cr, col*CELL_SIZE, (row+1)*CELL_SIZE);
      cairo_line_to(cr, (col+1)*CELL_SIZE, (row+1)*CELL_SIZE);

      /*right border*/
      cairo_move_to(cr, (col+1)*CELL_SIZE, row*CELL_SIZE);
      cairo_line_to(cr, (col+1)*CELL_SIZE, (row+1)*CELL_SIZE);

      cairo_stroke (cr);

      cairo_restore (cr);

      col = (col + 1) % COLUMNS;
      if (col == 0)
        row++;
    }

  /*left border*/
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, 0, (row+(col?1:0))*CELL_SIZE);

  /*top border*/
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, (row?COLUMNS:col)*CELL_SIZE, 0);

  cairo_stroke (cr);

  g_object_unref (context);
  g_object_unref (fontmap);
  g_object_unref (layout);

  gtk_widget_set_size_request (widget, COLUMNS*CELL_SIZE, (row+1)*CELL_SIZE);

  return TRUE;
}

/*  public functions  */

GtkWidget *
gimp_glyphs_panel_new (GimpContext *context)
{
   g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);

   return g_object_new (GIMP_TYPE_GLYPHS_PANEL,
                        "user-context", context,
                        NULL);
}
