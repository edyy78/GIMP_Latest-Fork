
#include <config.h>

#include <gtk/gtk.h>

#include "error_log.h"

//#include "widgets/gimptextbuffer.h"
// Not using gimp_text_buffer_new ().
// I don't know what it adds to gtk_text_buffer

GtkTextBuffer *error_log;

/*
 * Log of errors.
 *
 * Model for one or more views.
 */
GObject *
error_log_new()
{
  G_DEBUG_HERE ();

  error_log = gtk_text_buffer_new(NULL); /* Create new tag table. */
  gtk_text_buffer_create_tag (error_log, "title",
                              "scale",  PANGO_SCALE_LARGE,
                              "weight", PANGO_WEIGHT_BOLD,
                              NULL);
  gtk_text_buffer_create_tag (error_log, "message",
                              NULL);

  error_log_add ("Error log created.");

  return  G_OBJECT (error_log);
}

/* Returns GtkTextBuffer with contents the error log.
 * For now, error_log is-a GtkTextBuffer,
 * but future might be implemented say GStrv that can be inserted into a GtkTextBuffer.
 * The error log and GtkTextBuffer is a singleton.
 */
GObject *
error_log_get()
{
  G_DEBUG_HERE ();
  return  G_OBJECT (error_log);
}

void
error_log_add (gchar * message)
{
  GtkTextIter         end;

  G_DEBUG_HERE ();
  gtk_text_buffer_get_end_iter (error_log, &end);
  gtk_text_buffer_insert (error_log, &end, message, -1);
  gtk_text_buffer_insert (error_log, &end, "\n", -1);
}