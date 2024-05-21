/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpalternateglyphspopup.h
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

#ifndef GIMP_GLYPHS_ALTERNATE_POPUP_H
#define GIMP_GLYPHS_ALTERNATE_POPUP_H

#define GIMP_TYPE_GLYPHS_ALTERNATES_POPUP            (gimp_glyphs_alternates_popup_get_type ())
#define GIMP_GLYPHS_ALTERNATES_POPUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_GLYPHS_ALTERNATES_POPUP, GimpGlyphsAlternatesPopup))
#define GIMP_GLYPHS_ALTERNATES_POPUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_GLYPHS_ALTERNATES_POPUP, GimpGlyphsAlternatesPopupClass))
#define GIMP_IS_GLYPHS_ALTERNATES_POPUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_GLYPHS_ALTERNATES_POPUP))
#define GIMP_IS_GLYPHS_ALTERNATES_POPUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_GLYPHS_ALTERNATES_POPUP))
#define GIMP_GLYPHS_ALTERNATES_POPUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_GLYPHS_ALTERNATES_POPUP, GimpGlyphsAlternatesPopupClass))

typedef struct _GimpGlyphsAlternatesPopupClass GimpGlyphsAlternatesPopupClass;

GType       gimp_glyphs_alternates_popup_get_type (void) G_GNUC_CONST;

GtkWidget * gimp_glyphs_alternates_popup_new             (GimpContext               *context);
void        gimp_glyphs_alternates_popup_draw_selection  (GimpGlyphsAlternatesPopup *popup,
                                                          GimpFont                  *font,
                                                          gchar                     *text);


#endif /*GIMP_GLYPHS_ALTERNATE_POPUP_H*/
