/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * Copyright (C) 1997 Daniel Risacher
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

/*
 *   GUMP - Gimp Useless Mail Plugin
 *          (or Gump Useless Mail Plugin if you prefer)
 *
 *   by Adrian Likins <adrian@gimp.org>
 *      MIME encapsulation by Reagan Blundell <reagan@emails.net>
 *
 * As always: The utility of this plugin is left as an exercise for
 * the reader
 *
 */

#define _GNU_SOURCE

#include "config.h"

#include <fcntl.h>
#include <string.h>

#ifdef SENDMAIL
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <glib/gstdio.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include "libgimp/stdplugins-intl.h"

#define BUFFER_SIZE 256

#define PLUG_IN_PROC   "plug-in-mail-image"
#define PLUG_IN_BINARY "mail"
#define PLUG_IN_ROLE   "gimp-mail"


typedef struct _Mail      Mail;
typedef struct _MailClass MailClass;

struct _Mail
{
  GimpPlugIn      parent_instance;
};

struct _MailClass
{
  GimpPlugInClass parent_class;
};

#ifndef SENDMAIL /* org.freedesktop.portal.Email | xdg-email */
struct mail_data
{
  gchar *handle_token;
  gchar *parent_window;
  gchar *sender;
  gchar *response_handle;
  guint  response_signal_id;
  guint  response_result;
};
#endif

#define MAIL_TYPE  (mail_get_type ())
#define MAIL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), MAIL_TYPE, Mail))

GType                   mail_get_type         (void) G_GNUC_CONST;

static GList          * mail_init_procedures  (GimpPlugIn           *plug_in);
static GimpProcedure  * mail_create_procedure (GimpPlugIn           *plug_in,
                                               const gchar          *name);

static GimpValueArray * mail_run              (GimpProcedure        *procedure,
                                               GimpRunMode           run_mode,
                                               GimpImage            *image,
                                               GimpDrawable        **drawables,
                                               GimpProcedureConfig  *config,
                                               gpointer              run_data);

static GimpPDBStatusType  send_image          (GObject              *config,
                                               GimpImage            *image,
                                               GimpDrawable        **drawables,
                                               gint32                run_mode);

static gboolean           send_dialog             (GimpProcedure    *procedure,
                                                   GObject          *config);
static gboolean           valid_file              (GFile            *file);
static gchar            * find_extension          (const gchar      *filename);

#ifndef SENDMAIL /* org.freedesktop.portal.Email | xdg-email */
static void               compose_email_handle_response (GDBusConnection  *connection,
                                                         const gchar      *sender_name,
                                                         const gchar      *object_path,
                                                         const gchar      *interface_name,
                                                         const gchar      *signal_name,
                                                         GVariant         *parameters,
                                                         gpointer          user_data);
static void               compose_email_finish          (GObject          *source_object,
                                                         GAsyncResult     *res,
                                                         gpointer          user_data);
static gboolean           is_mail_portal_available      (void);
static gboolean           get_parent_window_handle      (gchar           **parent_window);
#else /* SENDMAIL */
static gchar            * sendmail_content_type   (const gchar      *filename);
static void               sendmail_create_headers (FILE             *mailpipe,
                                                   GObject          *config);
static gboolean           sendmail_to64           (const gchar      *filename,
                                                   FILE             *outfile,
                                                   GError          **error);
static FILE             * sendmail_pipe           (gchar           **cmd,
                                                   GPid             *pid);
#endif


G_DEFINE_TYPE (Mail, mail, GIMP_TYPE_PLUG_IN)

GIMP_MAIN (MAIL_TYPE)
DEFINE_STD_SET_I18N


#ifndef SENDMAIL /* org.freedesktop.portal.Email | xdg-email */
static GDBusProxy *proxy = NULL;
static GMainLoop  *loop  = NULL;
#else /* SENDMAIL */
static gchar *mesg_body = NULL;
#endif


static void
mail_class_init (MailClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->init_procedures  = mail_init_procedures;
  plug_in_class->create_procedure = mail_create_procedure;
  plug_in_class->set_i18n         = STD_SET_I18N;
}

static void
mail_init (Mail *mail)
{
}

static GList *
mail_init_procedures (GimpPlugIn *plug_in)
{
  GList    *list      = NULL;
  gchar    *email_bin = NULL;
  gboolean  available = FALSE;

  /* Check if sendmail is installed.
   * TODO: allow setting the location of the executable in preferences.
   */
#ifdef SENDMAIL
  if (strlen (SENDMAIL) == 0)
    {
      email_bin = g_find_program_in_path ("sendmail");
    }
  else
    {
      /* If a directory has been set at build time, we assume that sendmail
       * can only be in this directory. */
      email_bin = g_build_filename (SENDMAIL, "sendmail", NULL);
      if (! g_file_test (email_bin, G_FILE_TEST_IS_EXECUTABLE))
        {
          g_free (email_bin);
          email_bin = NULL;
        }
    }
#else
  email_bin = g_find_program_in_path ("xdg-email");
  if (is_mail_portal_available ())
    available = TRUE;
#endif

  if (email_bin) {
    g_free (email_bin);
    available = TRUE;
  }

  if (available)
    list = g_list_append (list, g_strdup (PLUG_IN_PROC));

  return list;
}

static GimpProcedure *
mail_create_procedure (GimpPlugIn  *plug_in,
                       const gchar *name)
{
  GimpProcedure *procedure = NULL;

  if (! strcmp (name, PLUG_IN_PROC))
    {
      procedure = gimp_image_procedure_new (plug_in, name,
                                            GIMP_PDB_PROC_TYPE_PLUGIN,
                                            mail_run, NULL, NULL);

      gimp_procedure_set_image_types (procedure, "*");
      gimp_procedure_set_sensitivity_mask (procedure,
                                           GIMP_PROCEDURE_SENSITIVE_DRAWABLE  |
                                           GIMP_PROCEDURE_SENSITIVE_DRAWABLES |
                                           GIMP_PROCEDURE_SENSITIVE_NO_DRAWABLES);

      gimp_procedure_set_menu_label (procedure, _("Send by E_mail..."));
      gimp_procedure_set_icon_name (procedure, GIMP_ICON_EDIT);
      gimp_procedure_add_menu_path (procedure, "<Image>/File/[Send]");

      gimp_procedure_set_documentation (procedure,
                                        _("Send the image by email"),
#ifdef SENDMAIL
                                        _("Sendmail is used to send emails "
                                          "and must be properly configured."),
#else /* org.freedesktop.portal.Email | xdg-email */
                                        _("The preferred email composer is "
                                          "used to send emails and must be "
                                          "properly configured."),
#endif
                                        name);
      gimp_procedure_set_attribution (procedure,
                                      "Adrian Likins, Reagan Blundell",
                                      "Adrian Likins, Reagan Blundell, "
                                      "Daniel Risacher, "
                                      "Spencer Kimball and Peter Mattis",
                                      "1995-1997");

      gimp_procedure_add_string_argument (procedure, "filename",
                                          _("File_name"),
                                          _("The name of the file to save the image in"),
                                          NULL,
                                          G_PARAM_READWRITE);

      gimp_procedure_add_string_argument (procedure, "to-address",
                                          _("_To"),
                                          _("The email address to send to"),
                                          "",
                                          G_PARAM_READWRITE);

      gimp_procedure_add_string_argument (procedure, "from-address",
                                          _("_From"),
                                          _("The email address for the From: field"),
                                          "",
                                          G_PARAM_READWRITE);

      gimp_procedure_add_string_argument (procedure, "subject",
                                          _("Su_bject"),
                                          _("The subject"),
                                          "",
                                          G_PARAM_READWRITE);

      gimp_procedure_add_string_argument (procedure, "comment",
                                          _("Co_mment"),
                                          _("The comment"),
                                          NULL,
                                          G_PARAM_READWRITE);
    }

  return procedure;
}

static GimpValueArray *
mail_run (GimpProcedure        *procedure,
          GimpRunMode           run_mode,
          GimpImage            *image,
          GimpDrawable        **drawables,
          GimpProcedureConfig  *config,
          gpointer              run_data)
{
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      gchar *filename = g_file_get_path (gimp_image_get_file (image));

      if (filename)
        {
          gchar *basename = g_filename_display_basename (filename);
          gchar  buffername[BUFFER_SIZE];

          g_strlcpy (buffername, basename, BUFFER_SIZE);

          g_object_set (config,
                        "filename", buffername,
                        NULL);
          g_free (basename);
          g_free (filename);
        }

      if (! send_dialog (procedure, G_OBJECT (config)))
        return gimp_procedure_new_return_values (procedure, GIMP_PDB_CANCEL, NULL);
    }

  status = send_image (G_OBJECT (config), image, drawables, run_mode);

  return gimp_procedure_new_return_values (procedure, status, NULL);
}

static GimpPDBStatusType
send_image (GObject       *config,
            GimpImage     *image,
            GimpDrawable **drawables,
            gint32         run_mode)
{
  GimpPDBStatusType  status      = GIMP_PDB_SUCCESS;
  gchar             *ext;
  GFile             *tmpfile;
  gchar             *tmpname;
#ifndef SENDMAIL /* org.freedesktop.portal.Email | xdg-email */
  gchar             *mailcmd[9];
  gchar             *filepath    = NULL;
  GFile             *tmp_dir     = NULL;
  GFileEnumerator   *enumerator  = NULL;
  GUnixFDList       *fd_list     = NULL;
  struct mail_data   mail_data   = {0};
  gint               fd          = -1;
#else /* SENDMAIL */
  gchar             *mailcmd[3];
  GPid               mailpid;
  FILE              *mailpipe    = NULL;
#endif
  GError            *error       = NULL;
  gchar             *filename    = NULL;
  gchar             *receipt     = NULL;
  gchar             *from        = NULL;
  gchar             *subject     = NULL;
  gchar             *comment     = NULL;

  mailcmd[0] = NULL;
  g_object_get (config,
                "filename",     &filename,
                "to-address",   &receipt,
                "from-address", &from,
                "subject",      &subject,
                "comment",      &comment,
                NULL);

  ext = find_extension (filename);

  if (ext == NULL)
    return GIMP_PDB_CALLING_ERROR;

  /* get a temp name with the right extension and save into it. */
  tmpfile = gimp_temp_file (ext + 1);
  tmpname = g_file_get_path (tmpfile);

  if (! (gimp_file_save (run_mode, image, tmpfile, NULL) &&
         valid_file (tmpfile)))
    {
      goto error;
    }

#ifndef SENDMAIL /* org.freedesktop.portal.Email | xdg-email */
  /* From xdg-email doc; relevant for org.freedesktop.portal.Email as well:
   * "Some e-mail applications require the file to remain present
   * after xdg-email returns."
   * As a consequence, the file cannot be removed at the end of the
   * function. We actually have no way to ever know *when* the file can
   * be removed since the caller could leave the email window opened for
   * hours. Yet we still want to clean sometimes and not have temporary
   * images piling up.
   * So I use a known directory that we control under $GIMP_DIRECTORY/tmp/,
   * and clean it out each time the plugin runs. This means that *if* you
   * are in the above case (your email client requires the file to stay
   * alive), you cannot run twice the plugin at the same time.
   */
  tmp_dir = gimp_directory_file ("tmp", PLUG_IN_PROC, NULL);

  if (g_mkdir_with_parents (gimp_file_get_utf8_name (tmp_dir),
                            S_IRUSR | S_IWUSR | S_IXUSR) == -1)
    {
      g_message ("Temporary directory %s could not be created.",
                 gimp_file_get_utf8_name (tmp_dir));
      g_error_free (error);
      goto error;
    }

  enumerator = g_file_enumerate_children (tmp_dir,
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          NULL, NULL);
  if (enumerator)
    {
      GFileInfo *info;

      while ((info = g_file_enumerator_next_file (enumerator,
                                                  NULL, NULL)))
        {
          if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_REGULAR)
            {
              GFile *file = g_file_enumerator_get_child (enumerator, info);
              g_file_delete (file, NULL, NULL);
              g_object_unref (file);
            }

          g_object_unref (info);
        }

      g_object_unref (enumerator);
    }

  filepath = g_build_filename (gimp_file_get_utf8_name (tmp_dir), filename,
                               NULL);
  if (g_rename (tmpname, filepath) == -1)
    {
      /* But on some system, I got an 'Invalid cross-device link' errno
       * with g_rename().
       * On the other hand, g_file_move() seems to be more robust.
       */
      GFile *source = tmpfile;
      GFile *target = g_file_new_for_path (filepath);

      if (! g_file_move (source, target, G_FILE_COPY_NONE, NULL, NULL, NULL, &error))
        {
          g_message ("%s", error->message);
          g_clear_error (&error);
          g_object_unref (target);
          goto error;
        }

      g_object_unref (target);
    }

  if (is_mail_portal_available () && get_parent_window_handle (&mail_data.parent_window))
    {
      GVariantBuilder compose_email_opts;
      GVariantBuilder attach_fds;
      gint            fd_in;

      mail_data.handle_token = g_strdup_printf ("gimp%d", g_random_int_range (0, G_MAXINT));
      mail_data.sender = g_strdelimit (g_strdup (g_dbus_connection_get_unique_name (g_dbus_proxy_get_connection (proxy)) + 1),
                                       ".", '_');

      mail_data.response_handle = g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s",
                                                   mail_data.sender, mail_data.handle_token);
      mail_data.response_signal_id = g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (proxy),
                                                                         NULL,
                                                                         "org.freedesktop.portal.Request",
                                                                         "Response",
                                                                         mail_data.response_handle,
                                                                         NULL,
                                                                         G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                                         compose_email_handle_response,
                                                                         &mail_data,
                                                                         NULL);

      g_variant_builder_init (&compose_email_opts, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&compose_email_opts, "{sv}", "handle_token",
                             g_variant_new_string (mail_data.handle_token));
      if (receipt != NULL && strlen (receipt) > 0)
        g_variant_builder_add (&compose_email_opts, "{sv}", "address",
                               g_variant_new_string (receipt));
      if (subject != NULL && strlen (subject) > 0)
        g_variant_builder_add (&compose_email_opts, "{sv}", "subject",
                               g_variant_new_string (subject));
      if (comment != NULL && strlen (comment) > 0)
        g_variant_builder_add (&compose_email_opts, "{sv}", "body",
                               g_variant_new_string (comment));

      fd_list = g_unix_fd_list_new ();
      g_variant_builder_init (&attach_fds, G_VARIANT_TYPE ("ah"));

      fd = g_open (filepath, O_PATH | O_CLOEXEC);
      if (fd == -1)
        {
          g_warning ("Failed to open %s: %s", tmpname, g_strerror (errno));
          goto error;
        }

      fd_in = g_unix_fd_list_append (fd_list, fd, &error);
      if (fd_in == -1)
        {
          g_warning ("%s", error->message);
          g_error_free (error);
          goto error;
        }

      g_variant_builder_add (&attach_fds, "h", fd_in);
      g_variant_builder_add (&compose_email_opts, "{sv}", "attachment_fds",
                             g_variant_builder_end (&attach_fds));

      loop = g_main_loop_new (NULL, TRUE);

      g_dbus_proxy_call_with_unix_fd_list (proxy,
                                           "ComposeEmail",
                                           g_variant_new ("(sa{sv})", mail_data.parent_window, &compose_email_opts),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           G_MAXINT,
                                           fd_list  ,
                                           NULL,
                                           compose_email_finish,
                                           &mail_data);

      g_main_loop_run (loop);
    }
  else
    {
      gint i = 0;

      mailcmd[i++] = g_strdup ("xdg-email");
      mailcmd[i++] = "--attach";
      mailcmd[i++] = filepath;
      if (subject != NULL && strlen (subject) > 0)
        {
          mailcmd[i++] = "--subject";
          mailcmd[i++] = subject;
        }
      if (comment != NULL && strlen (comment) > 0)
        {
          mailcmd[i++] = "--body";
          mailcmd[i++] = comment;
        }
      if (receipt != NULL && strlen (receipt) > 0)
        {
          mailcmd[i++] = receipt;
        }
      mailcmd[i] = NULL;

      if (! g_spawn_async (NULL, mailcmd, NULL,
                           G_SPAWN_SEARCH_PATH,
                           NULL, NULL, NULL, &error))
       {
         g_message ("%s", error->message);
         g_error_free (error);
         goto error;
       }
    }
#else /* SENDMAIL */
  /* construct the "sendmail user@location" line */
  if (strlen (SENDMAIL) == 0)
    mailcmd[0] = g_strdup ("sendmail");
  else
    mailcmd[0] = g_build_filename (SENDMAIL, "sendmail", NULL);

  mailcmd[1] = receipt;
  mailcmd[2] = NULL;

  /* create a pipe to sendmail */
  mailpipe = sendmail_pipe (mailcmd, &mailpid);

  if (mailpipe == NULL)
    return GIMP_PDB_EXECUTION_ERROR;

  sendmail_create_headers (mailpipe, config);

  fflush (mailpipe);

  if (! sendmail_to64 (tmpname, mailpipe, &error))
    {
      g_message ("%s", error->message);
      g_error_free (error);
      goto error;
    }

  fprintf (mailpipe, "\n--GUMP-MIME-boundary--\n");
#endif

  goto cleanup;

error:
  /* stop sendmail from doing anything */
#ifdef SENDMAIL
  kill (mailpid, SIGINT);
#endif
  status = GIMP_PDB_EXECUTION_ERROR;

cleanup:
  /* close out the sendmail process */
#ifdef SENDMAIL
  if (mailpipe)
    {
      fclose (mailpipe);
      waitpid (mailpid, NULL, 0);
      g_spawn_close_pid (mailpid);
    }

#else /* org.freedesktop.portal.Email | xdg-email */
  g_clear_object (&fd_list);
  g_free (mail_data.handle_token);
  g_free (mail_data.sender);
  g_free (mail_data.response_handle);
  if (fd != -1)
    g_close (fd, NULL);
  if (mail_data.response_signal_id != 0)
    g_dbus_connection_signal_unsubscribe (g_dbus_proxy_get_connection (proxy),
                                          mail_data.response_signal_id);
#endif

  g_free (filename);
  g_free (receipt);
  g_free (from);
  g_free (subject);
  g_free (comment);

  g_free (mailcmd[0]);
  /* delete the tmpfile that was generated */
  g_unlink (tmpname);
  g_free (tmpname);
  g_object_unref (tmpfile);
  return status;
}


static gboolean
send_dialog (GimpProcedure *procedure,
             GObject       *config)
{
  GtkWidget     *dlg;
  GtkWidget     *main_vbox;
  GtkWidget     *entry;
  GtkWidget     *real_entry;
  GtkWidget     *grid;
  GtkWidget     *button;
#ifdef SENDMAIL
  GtkWidget     *scrolled_window;
  GtkWidget     *text_view;
#endif
  gchar         *gump_from;
  gboolean       run;

  gimp_ui_init (PLUG_IN_BINARY);

  /* check gimprc for a preferred "From:" address */
  gump_from = gimp_gimprc_query ("gump-from");

  if (gump_from)
    {
      g_object_set (config,
                    "from-address", gump_from,
                    NULL);
      g_free (gump_from);
    }

  dlg = gimp_procedure_dialog_new (procedure,
                                   GIMP_PROCEDURE_CONFIG (config),
                                   _("Send by Email"));
  /* Change "Ok" button to "Send" */
  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dlg),
                                               GTK_RESPONSE_OK);
  gtk_button_set_label (GTK_BUTTON (button), _("Send"));


  gimp_dialog_set_alternative_button_order (GTK_DIALOG (dlg),
                                            GTK_RESPONSE_OK,
                                            GTK_RESPONSE_CANCEL,
                                            -1);

  gimp_window_set_transient (GTK_WINDOW (dlg));

  main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
                      main_vbox, TRUE, TRUE, 0);
  gtk_widget_show (main_vbox);

  /* grid */
  grid = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (main_vbox), grid, FALSE, FALSE, 0);
  gtk_widget_show (grid);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

  /* Filename entry */
  entry = gimp_procedure_dialog_get_widget (GIMP_PROCEDURE_DIALOG (dlg),
                                            "filename", GIMP_TYPE_LABEL_ENTRY);
  real_entry = gimp_label_entry_get_entry (GIMP_LABEL_ENTRY (entry));
  gtk_widget_set_size_request (real_entry, 200, -1);
  gtk_entry_set_activates_default (GTK_ENTRY (real_entry), TRUE);

  gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dlg),
                              "filename",
                              NULL);
  gtk_entry_set_max_length (GTK_ENTRY (real_entry),
                            BUFFER_SIZE - 1);

#ifdef SENDMAIL
  /* To entry */
  entry = gimp_procedure_dialog_get_widget (GIMP_PROCEDURE_DIALOG (dlg),
                                            "to-address",
                                            GIMP_TYPE_LABEL_ENTRY);
  real_entry = gimp_label_entry_get_entry (GIMP_LABEL_ENTRY (entry));
  gtk_widget_set_size_request (real_entry, 200, -1);
  gtk_entry_set_max_length (GTK_ENTRY (real_entry), BUFFER_SIZE - 1);
  gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dlg),
                              "to-address",
                              NULL);
  gtk_entry_set_max_length (GTK_ENTRY (real_entry),
                            BUFFER_SIZE - 1);

  gtk_widget_grab_focus (real_entry);

  /* From entry */
  entry = gimp_procedure_dialog_get_widget (GIMP_PROCEDURE_DIALOG (dlg),
                                            "from-address",
                                            GIMP_TYPE_LABEL_ENTRY);
  real_entry = gimp_label_entry_get_entry (GIMP_LABEL_ENTRY (entry));
  gtk_widget_set_size_request (real_entry, 200, -1);
  gtk_entry_set_max_length (GTK_ENTRY (real_entry), BUFFER_SIZE - 1);
  gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dlg),
                              "from-address",
                              NULL);
  gtk_entry_set_max_length (GTK_ENTRY (real_entry),
                            BUFFER_SIZE - 1);

  /* Subject entry */
  entry = gimp_procedure_dialog_get_widget (GIMP_PROCEDURE_DIALOG (dlg),
                                            "subject",
                                            GIMP_TYPE_LABEL_ENTRY);
  real_entry = gimp_label_entry_get_entry (GIMP_LABEL_ENTRY (entry));
  gtk_widget_set_size_request (real_entry, 200, -1);
  gtk_entry_set_max_length (GTK_ENTRY (real_entry), BUFFER_SIZE - 1);
  gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dlg),
                              "subject",
                              NULL);
  gtk_entry_set_max_length (GTK_ENTRY (real_entry),
                            BUFFER_SIZE - 1);

  /* Body  */
  text_view = gimp_procedure_dialog_get_widget (GIMP_PROCEDURE_DIALOG (dlg),
                                                "comment",
                                                GTK_TYPE_TEXT_VIEW);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_WORD);

  scrolled_window =
    gimp_procedure_dialog_fill_scrolled_window (GIMP_PROCEDURE_DIALOG (dlg),
                                                "comment-scrolled",
                                                "comment");
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dlg),
                              "comment-scrolled",
                              NULL);
#endif

  gtk_widget_show (dlg);

  run = gimp_procedure_dialog_run (GIMP_PROCEDURE_DIALOG (dlg));

  gtk_widget_destroy (dlg);

  return run;
}

static gboolean
valid_file (GFile *file)
{
  GStatBuf  buf;
  gboolean  valid;

  valid = g_stat (g_file_peek_path (file), &buf) == 0 && buf.st_size > 0;

  return valid;
}

static gchar *
find_extension (const gchar *filename)
{
  gchar *filename_copy;
  gchar *ext;

  /* we never free this copy - aren't we evil! */
  filename_copy = g_strdup (filename);

  /* find the extension, boy! */
  ext = strrchr (filename_copy, '.');

  while (TRUE)
    {
      if (!ext || ext[1] == '\0' || strchr (ext, G_DIR_SEPARATOR))
        {
          g_message (_("some sort of error with the file extension "
                       "or lack thereof"));

          return NULL;
        }

      if (0 != g_ascii_strcasecmp (ext, ".gz") &&
          0 != g_ascii_strcasecmp (ext, ".bz2"))
        {
          return ext;
        }
      else
        {
          /* we found something, loop back, and look again */
          *ext = 0;
          ext = strrchr (filename_copy, '.');
        }
    }

  g_free (filename_copy);

  return ext;
}

#ifndef SENDMAIL /* org.freedesktop.portal.Email | xdg-email */
static void
compose_email_handle_response (GDBusConnection *connection,
                               const gchar     *sender_name,
                               const gchar     *object_path,
                               const gchar     *interface_name,
                               const gchar     *signal_name,
                               GVariant        *parameters,
                               gpointer         user_data)
{
  GVariant         *results = NULL;
  struct mail_data *data    = (struct mail_data *) user_data;

  g_variant_get (parameters, "(ua{sv})",
                 &data->response_result,
                 &results);

  g_dbus_connection_signal_unsubscribe (g_dbus_proxy_get_connection (proxy),
                                        data->response_signal_id);
  data->response_signal_id = 0;

  g_main_loop_quit (loop);
}

static void
compose_email_finish (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GError           *error           = NULL;
  GVariant         *results         = NULL;
  gchar            *response_handle = NULL;
  struct mail_data *data            = (struct mail_data *) user_data;

  results = g_dbus_proxy_call_with_unix_fd_list_finish (proxy,
                                                        NULL,
                                                        res,
                                                        &error);

  if (error != NULL)
    {
      g_warning ("There was a problem while calling the email portal: %s", error->message);
      g_free (error);
      return;
    }

  g_variant_get (results, "(&o)", &response_handle);

  if (g_strcmp0 (response_handle, data->response_handle) == 0)
    {
      g_free (response_handle);
      return;
    }

  g_dbus_connection_signal_unsubscribe (g_dbus_proxy_get_connection (proxy),
                                        data->response_signal_id);

  g_free (data->response_handle);
  data->response_handle = response_handle;

  data->response_signal_id = g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (proxy),
                                                                 NULL,
                                                                 "org.freedesktop.portal.Request",
                                                                 "Response",
                                                                 response_handle,
                                                                 NULL,
                                                                 G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                                 compose_email_handle_response,
                                                                 data,
                                                                 NULL);
}

static gboolean
is_mail_portal_available (void)
{
  g_autoptr (GVariant) prop = NULL;
  guint32              version;

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE,
                                         NULL, "org.freedesktop.portal.Desktop",
                                         "/org/freedesktop/portal/desktop",
                                         "org.freedesktop.portal.Email", NULL, NULL);

  prop = g_dbus_proxy_get_cached_property (proxy, "version");
  g_variant_get (prop, "u", &version);
  if (version < 4)
    {
      g_info ("Email portal version too old (%d, need 4)", version);
      return FALSE;
    }

  return TRUE;
}

static gboolean
get_parent_window_handle (gchar **parent_window)
{
#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND)
  GBytes *handle;

  handle = gimp_progress_get_window_handle ();
#endif

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      GdkWindow *window      = NULL;
      guint32   *handle_data = NULL;
      guint32    window_id;
      gsize      handle_size;

      handle_data = (guint32 *) g_bytes_get_data (handle, &handle_size);
      g_return_val_if_fail (handle_size == sizeof (guint32), FALSE);
      window_id = *handle_data;

      window = gdk_x11_window_foreign_new_for_display (gdk_display_get_default (), window_id);
      if (window)
        {
          gint id;

          id            = GDK_WINDOW_XID (window);
          *parent_window = g_strdup_printf ("x11:0x%x", id);
        }
    }
#endif

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      char  *handle_data;
      gchar *handle_str;
      gsize  handle_size;

      handle_data   = (char *) g_bytes_get_data (handle, &handle_size);
      /* Even though this should be the case by design, this ensures the
       * string is NULL-terminated to avoid out-of allocated memory access.
       */
      handle_str    = g_strndup (handle_data, handle_size);
      *parent_window = g_strdup_printf ("wayland:%s", handle_str);
      g_free (handle_str);
    }
#endif

#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND)
  g_bytes_unref (handle);
#endif

  return TRUE;
}
#else /* SENDMAIL */
static gchar *
sendmail_content_type (const gchar *filename)
{
  /* This function returns a MIME Content-type: value based on the
     filename it is given.  */
  const gchar *type_mappings[20] =
  {
    "gif" , "image/gif",
    "jpg" , "image/jpeg",
    "jpeg", "image/jpeg",
    "tif" , "image/tiff",
    "tiff", "image/tiff",
    "png" , "image/png",
    "g3"  , "image/g3fax",
    "ps"  , "application/postscript",
    "eps" , "application/postscript",
    NULL, NULL
  };

  gchar *ext;
  gint   i;

  ext = find_extension (filename);

  if (!ext)
    {
      return g_strdup ("application/octet-stream");
    }

  i = 0;
  ext += 1;

  while (type_mappings[i])
    {
      if (g_ascii_strcasecmp (ext, type_mappings[i]) == 0)
        {
          return g_strdup (type_mappings[i + 1]);
        }

      i += 2;
    }

  return g_strdup_printf ("image/x-%s", ext);
}

static void
sendmail_create_headers (FILE    *mailpipe,
                         GObject *config)
{
  gchar *filename = NULL;
  gchar *receipt  = NULL;
  gchar *from     = NULL;
  gchar *subject  = NULL;
  gchar *comment  = NULL;

  g_object_get (config,
                "filename",     &filename,
                "to-address",   &receipt,
                "from-address", &from,
                "subject",      &subject,
                "comment",      &comment,
                NULL);

  /* create all the mail header stuff. Feel free to add your own */
  /* It is advisable to leave the X-Mailer header though, as     */
  /* there is a possibility of a Gimp mail scanner/reader in the  */
  /* future. It will probably need that header.                 */

  fprintf (mailpipe, "To: %s \n", receipt);
  fprintf (mailpipe, "Subject: %s \n", subject);
  if (from != NULL && strlen (from) > 0)
    fprintf (mailpipe, "From: %s \n", from);

  fprintf (mailpipe, "X-Mailer: GIMP Useless Mail plug-in %s\n", GIMP_VERSION);

  fprintf (mailpipe, "MIME-Version: 1.0\n");
  fprintf (mailpipe, "Content-type: multipart/mixed; "
                     "boundary=GUMP-MIME-boundary\n");

  fprintf (mailpipe, "\n\n");

  fprintf (mailpipe, "--GUMP-MIME-boundary\n");
  fprintf (mailpipe, "Content-type: text/plain; charset=UTF-8\n\n");

  if (mesg_body)
    fprintf (mailpipe, "%s", mesg_body);

  fprintf (mailpipe, "\n\n");

  {
    gchar *content = sendmail_content_type (filename);

    fprintf (mailpipe, "--GUMP-MIME-boundary\n");
    fprintf (mailpipe, "Content-type: %s\n", content);
    fprintf (mailpipe, "Content-transfer-encoding: base64\n");
    fprintf (mailpipe, "Content-disposition: attachment; filename=\"%s\"\n",
             filename);
    fprintf (mailpipe, "Content-description: %s\n\n", filename);

    g_free (content);
  }

  g_free (filename);
  g_free (receipt);
  g_free (from);
  g_free (subject);
  g_free (comment);
}

static gboolean
sendmail_to64 (const gchar  *filename,
               FILE         *outfile,
               GError      **error)
{
  GMappedFile  *infile;
  const guchar *in;
  gchar         out[2048];
  gint          state = 0;
  gint          save  = 0;
  gsize         len;
  gsize         bytes;
  gsize         c;

  infile = g_mapped_file_new (filename, FALSE, error);
  if (! infile)
    return FALSE;

  in = (const guchar *) g_mapped_file_get_contents (infile);
  len = g_mapped_file_get_length (infile);

  for (c = 0; c < len;)
    {
      gsize step = MIN (1024, len - c);

      bytes = g_base64_encode_step (in + c, step, TRUE, out, &state, &save);
      fwrite (out, 1, bytes, outfile);

      c += step;
    }

  bytes = g_base64_encode_close (TRUE, out, &state, &save);
  fwrite (out, 1, bytes, outfile);

  g_mapped_file_unref (infile);

  return TRUE;
}

static FILE *
sendmail_pipe (gchar **cmd,
               GPid   *pid)
{
  gint    fd;
  GError *err = NULL;

  if (! g_spawn_async_with_pipes (NULL, cmd, NULL,
                                  G_SPAWN_DO_NOT_REAP_CHILD |
                                  G_SPAWN_SEARCH_PATH,
                                  NULL, NULL, pid, &fd, NULL, NULL, &err))
    {
      g_message (_("Could not start sendmail (%s)"), err->message);
      g_error_free (err);

      *pid = -1;
      return NULL;
    }

  return fdopen (fd, "wb");
}
#endif
