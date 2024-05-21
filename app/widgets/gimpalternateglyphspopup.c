/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpalternateglyphspopup.c
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

#include "core/gimpcontext.h"

#include "text/gimpfont.h"

#include "gimpoverlayframe.h"
#include "gimpalternateglyphspopup.h"

#define COLUMNS      4
#define CELL_SIZE    50

#define MAX_WIDTH   200
#define MAX_HEIGHT  200

static gboolean gimp_alternate_glyphs_popup_draw_overlay         (GtkWidget                 *widget,
                                                                  cairo_t                   *cr);
static void     gimp_alternate_glyphs_popup_finalize             (GObject                   *object);
static void     gimp_glyphs_alternates_popup_set_property        (GObject                   *object,
                                                                  guint                      property_id,
                                                                  const GValue              *value,
                                                                  GParamSpec                *pspec);
static gboolean gimp_glyphs_alternates_popup_draw                (GtkWidget                 *widget,
                                                                  cairo_t                   *cr,
                                                                  GimpGlyphsAlternatesPopup *popup);
static gboolean gimp_glyphs_alternates_popup_button_press_event  (GtkWidget                 *widget,
                                                                  GdkEvent                  *event,
                                                                  GimpGlyphsAlternatesPopup *popup);

enum
{
  PROP_0,
  PROP_GIMP_CONTEXT,
  N_PROPS
};

struct _GimpGlyphsAlternatesPopup
{
  GimpOverlayFrame  parent_instance;

  GtkWidget        *alternates;
  GtkWidget        *alternates_window;
  GimpFont         *font;
  GList            *selection_alternates;
  GimpContext      *context;
  gchar            *text;
  gint              selection;
};

struct _GimpGlyphsAlternatesPopupClass
{
  GtkScrolledWindowClass parent_class;
};

G_DEFINE_TYPE (GimpGlyphsAlternatesPopup, gimp_glyphs_alternates_popup, GIMP_TYPE_OVERLAY_FRAME)

#define parent_class gimp_glyphs_alternates_popup_parent_class

static void
gimp_glyphs_alternates_popup_class_init (GimpGlyphsAlternatesPopupClass *klass)
{
   GObjectClass   *object_class = G_OBJECT_CLASS (klass);
   GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

   object_class->set_property = gimp_glyphs_alternates_popup_set_property;
   object_class->finalize     = gimp_alternate_glyphs_popup_finalize;
   widget_class->draw         = gimp_alternate_glyphs_popup_draw_overlay;

   g_object_class_install_property (object_class, PROP_GIMP_CONTEXT,
                                    g_param_spec_object ("context",
                                                         NULL, NULL,
                                                         GIMP_TYPE_CONTEXT,
                                                         G_PARAM_STATIC_STRINGS |
                                                         G_PARAM_WRITABLE       |
                                                         G_PARAM_CONSTRUCT_ONLY));
}

static void
gimp_glyphs_alternates_popup_set_property (GObject      *object,
                                           guint         property_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  GimpGlyphsAlternatesPopup *popup =  GIMP_GLYPHS_ALTERNATES_POPUP (object);

  switch (property_id)
  {
    case PROP_GIMP_CONTEXT:
      popup->context = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gimp_glyphs_alternates_popup_init (GimpGlyphsAlternatesPopup *popup)
{
  popup->alternates_window = gtk_scrolled_window_new (NULL, NULL);
  popup->alternates = gtk_drawing_area_new ();

  gtk_container_add (GTK_CONTAINER (popup), popup->alternates_window);
  gtk_container_add (GTK_CONTAINER (popup->alternates_window), popup->alternates);

  gtk_widget_show (GTK_WIDGET (popup));
  gtk_widget_show (GTK_WIDGET (popup->alternates_window));
  gtk_widget_show (GTK_WIDGET (popup->alternates));

  g_signal_connect (popup->alternates, "draw",
                    G_CALLBACK (gimp_glyphs_alternates_popup_draw),
                    popup);

  gtk_widget_add_events (GTK_WIDGET (popup->alternates), GDK_BUTTON_PRESS_MASK);

  g_signal_connect (G_OBJECT (popup->alternates), "button-press-event",
                    G_CALLBACK (gimp_glyphs_alternates_popup_button_press_event), popup);

}

static gboolean
gimp_glyphs_alternates_popup_button_press_event  (GtkWidget                 *widget,
                                                  GdkEvent                  *event,
                                                  GimpGlyphsAlternatesPopup *popup)
{
  gdouble y;
  gdouble x;
  guint   row;
  guint   col;
  guint   entry_pos;
  GList   *list;

  if (((GdkEventButton*)event)->button != GDK_BUTTON_PRIMARY)
    return 1;

  y         = ((GdkEventButton*)event)->x;
  x         = ((GdkEventButton*)event)->y;
  row       = x/CELL_SIZE;
  col       = y/CELL_SIZE;
  entry_pos = row*COLUMNS+col;
  list      = popup->selection_alternates;

  for (gint i = 0; list && i < entry_pos; i++)
    list = list->next;

  if (list)
    {
      popup->selection = entry_pos;
      gtk_widget_queue_draw (popup->alternates);
      g_signal_emit_by_name (popup->context, "glyph-changed", (gchar*)(list->data));
    }

  return 0;
}

static gboolean
gimp_alternate_glyphs_popup_draw_overlay (GtkWidget *widget,
                                          cairo_t   *cr)
{
  GimpGlyphsAlternatesPopup *popup = GIMP_GLYPHS_ALTERNATES_POPUP (widget);

  popup->selection_alternates = gimp_font_get_all_string_substitutes (popup->font, popup->text);

  if (g_list_length (popup->selection_alternates) == 0)
    return TRUE;

  return GTK_WIDGET_CLASS (parent_class)->draw (widget, cr);
}

static gboolean
gimp_glyphs_alternates_popup_draw (GtkWidget                 *widget,
                                   cairo_t                   *cr,
                                   GimpGlyphsAlternatesPopup *popup)
{
  guint          row           = 0;
  guint          col           = 0;
  PangoFontMap  *fontmap       = NULL;
  PangoContext  *pango_context = NULL;
  PangoLayout   *layout        = NULL;

  if (g_list_length (popup->selection_alternates) == 0)
    return TRUE;

  fontmap       = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
  pango_context = pango_font_map_create_context (fontmap);
  layout        = pango_layout_new (pango_context);

  cairo_set_source_rgb(cr, 1, 1, 1); /* white for the background */
  cairo_paint(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); /* black color for text */

  for (GList *alternates = popup->selection_alternates; alternates; alternates = alternates->next)
    {
      cairo_rectangle (cr, col*CELL_SIZE, row*CELL_SIZE, CELL_SIZE, CELL_SIZE);

      pango_layout_set_markup (layout, (gchar*)(alternates->data), -1);

      cairo_save (cr);

      cairo_clip(cr);

      cairo_move_to(cr, col*CELL_SIZE+CELL_SIZE/3, row*CELL_SIZE+CELL_SIZE/4);
      pango_cairo_show_layout (cr, layout);

      /*bottom border*/
      cairo_move_to(cr, col*CELL_SIZE, (row+1)*CELL_SIZE);
      cairo_line_to(cr, (col+1)*CELL_SIZE, (row+1)*CELL_SIZE);

      /*right border*/
      cairo_move_to(cr, (col+1)*CELL_SIZE, row*CELL_SIZE);
      cairo_line_to(cr, (col+1)*CELL_SIZE, (row+1)*CELL_SIZE);

      cairo_stroke(cr);

      cairo_restore (cr);

      col = (col+1) % COLUMNS;
      if (col == 0)
        row++;
    }

  /*left border*/
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 0, (row+(col?1:0))*CELL_SIZE);

  /*top border*/
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, (row?COLUMNS:col)*CELL_SIZE, 0);

  cairo_stroke(cr);

  gtk_widget_set_size_request (GTK_WIDGET (popup->alternates_window),
                               MIN (MAX_WIDTH, (!row?col:COLUMNS)*CELL_SIZE),
                               MIN (MAX_HEIGHT, (row+1)*CELL_SIZE));
  gtk_widget_set_size_request (widget, (!row?col:COLUMNS)*CELL_SIZE, (row+1)*CELL_SIZE);

  g_object_unref (pango_context);
  g_object_unref (fontmap);
  g_object_unref (layout);

  return TRUE;
}

static void
gimp_alternate_glyphs_popup_finalize (GObject *object)
{
  GimpGlyphsAlternatesPopup *popup = GIMP_GLYPHS_ALTERNATES_POPUP (object);

  g_list_free_full (popup->selection_alternates, (GDestroyNotify)g_free);
  g_free (popup->text);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*  public functions  */

void
gimp_glyphs_alternates_popup_draw_selection (GimpGlyphsAlternatesPopup *popup,
                                             GimpFont                  *font,
                                             gchar                     *text)
{
  popup->font = font;
  popup->text = text;
  gtk_widget_queue_draw (popup->alternates);
}


GtkWidget *
gimp_glyphs_alternates_popup_new (GimpContext *context)
{
   g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);

   return g_object_new (GIMP_TYPE_GLYPHS_ALTERNATES_POPUP,
                        "context", context,
                        NULL);
}
