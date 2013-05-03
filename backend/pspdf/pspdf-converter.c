/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Ali Sabil <ali.sabil@gmail.com>
 *  Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *  Copyright 1998 - 2005 The Free Software Foundation
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gstdio.h>
#include <sys/wait.h>
#include <errno.h>

#include "pspdf-converter.h"

#define MAX_BUFSIZE 1024

enum { // signals
    CONVERSION_FINISHED,
    LAST_SIGNAL
};

static guint pspdf_signals[LAST_SIGNAL];

struct _PSPDFConverter {
    GObject parent_instance;

    GPid pid;               /* PID of converter, -1 if none  */
    GIOChannel *input;      /* stdin of converter            */
    GIOChannel *output;     /* stdout of converter           */
    GIOChannel *error;      /* stderr of converter           */
    guint input_id;
    guint output_id;
    guint error_id;

    gchar *filename;        /* the currently loaded filename */
    GString *pdfdata;       /* the conversion result */
    
};

struct _PSPDFConverterClass {
    GObjectClass parent_class;
    void (* conversion_finished) (PSPDFConverter *ps2pdf);
};

G_DEFINE_TYPE (PSPDFConverter, pspdf_converter, G_TYPE_OBJECT)


static void
pspdf_converter_dispose (GObject *object)
{
    PSPDFConverter *ps2pdf = PSPDF_CONVERTER (object);
    
    if (ps2pdf->pdfdata) {
        g_string_free(ps2pdf->pdfdata, TRUE);
        ps2pdf->pdfdata = NULL;
    }

    if (ps2pdf->filename) {
        g_free (ps2pdf->filename);
        ps2pdf->filename = NULL;
    }

    pspdf_converter_stop (ps2pdf);

    G_OBJECT_CLASS (pspdf_converter_parent_class)->dispose (object);
}

static void
pspdf_converter_init (PSPDFConverter *ps2pdf)
{
    ps2pdf->pid = -1;
    ps2pdf->pdfdata = g_string_new(NULL);
}

static void
pspdf_converter_class_init (PSPDFConverterClass *klass)
{
    GObjectClass *object_class;
    GParamSpec *pspec;

    object_class = G_OBJECT_CLASS (klass);

    pspdf_signals[CONVERSION_FINISHED] =
        g_signal_new ("conversion_finished",
                PSPDF_TYPE_CONVERTER,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (PSPDFConverterClass, conversion_finished),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0,
                NULL);
    
    object_class->dispose = pspdf_converter_dispose;
}

static void
pspdf_converter_failed (PSPDFConverter *ps2pdf, const char *msg)
{
    g_warning (msg ? msg : "PS2PDF conversion failed.");
    pspdf_converter_stop (ps2pdf);
}

static void
pspdf_converter_finished (GPid           pid,
        gint           status,
        PSPDFConverter *ps2pdf)
{
    g_spawn_close_pid (ps2pdf->pid);
    ps2pdf->pid = -1;
    pspdf_converter_stop (ps2pdf);
    g_signal_emit (ps2pdf, pspdf_signals[CONVERSION_FINISHED], 0, NULL);
}

static gboolean
pspdf_converter_output (GIOChannel    *io,
        GIOCondition   condition,
        PSPDFConverter *ps2pdf)
{
    gchar buf[MAX_BUFSIZE + 1];
    gsize bytes = 0;
    GIOStatus status;
    GError *error = NULL;

    status = g_io_channel_read_chars (io, buf, MAX_BUFSIZE, &bytes, &error);
    switch (status) {
        case G_IO_STATUS_NORMAL:
            if (bytes > 0) {
                ps2pdf->pdfdata = g_string_append_len (ps2pdf->pdfdata, buf, bytes);
            }
            break;
        case G_IO_STATUS_EOF:
            g_io_channel_unref (ps2pdf->output);
            ps2pdf->output = NULL;
            ps2pdf->output_id = 0;

            return FALSE;
        case G_IO_STATUS_ERROR:
            pspdf_converter_failed (ps2pdf, error->message);
            g_error_free (error);
            ps2pdf->output_id = 0;

            return FALSE;
        default:
            break;
    }

    if (!ps2pdf->error) {
            pspdf_converter_failed (ps2pdf, NULL);
    }

    return TRUE;
}

static gboolean
pspdf_converter_error (GIOChannel    *io,
        GIOCondition   condition,
        PSPDFConverter *ps2pdf)
{
    gchar buf[MAX_BUFSIZE + 1];
    gsize bytes = 0;
    GIOStatus status;
    GError *error = NULL;

    status = g_io_channel_read_chars (io, buf, MAX_BUFSIZE,
            &bytes, &error);
    switch (status) {
        case G_IO_STATUS_NORMAL:
            if (bytes > 0) {
                    buf[bytes] = '\0';
                    g_print ("%s", buf);
            }

            break;
        case G_IO_STATUS_EOF:
            g_io_channel_unref (ps2pdf->error);
            ps2pdf->error = NULL;
            ps2pdf->error_id = 0;

            return FALSE;
        case G_IO_STATUS_ERROR:
            pspdf_converter_failed (ps2pdf, error->message);
            g_error_free (error);
            ps2pdf->error_id = 0;

            break;
        default:
            break;
    }

    if (!ps2pdf->output) {
            pspdf_converter_failed (ps2pdf, NULL);
    }

    return TRUE;
}

/* Public methods */
PSPDFConverter *
pspdf_converter_new (const gchar *filename)
{
        PSPDFConverter *ps2pdf;

        g_return_val_if_fail (filename != NULL, NULL);

        ps2pdf = PSPDF_CONVERTER (g_object_new (PSPDF_TYPE_CONVERTER, NULL));
        ps2pdf->filename = g_strdup (filename);

        return ps2pdf;
}

#define NUM_ARGS    100
#define NUM_GS_ARGS (NUM_ARGS - 20)
#define NUM_PS2PDF_ARGS 10

void
pspdf_converter_start (PSPDFConverter *ps2pdf)
{
    gchar *argv[NUM_ARGS], *dir, *gs_path;
    gchar **gs_args, **ps2pdf_args = NULL;
    gint pin, pout, perr;
    gint argc = 0, i;
    GError *error = NULL;

    g_assert (ps2pdf->filename != NULL);
    g_assert (ps2pdf->pid == -1); // one converter per object

    pspdf_converter_stop (ps2pdf);

    dir = g_path_get_dirname (ps2pdf->filename);
    gs_path = g_find_program_in_path ("gs");
    gs_args = g_strsplit (gs_path, " ", NUM_GS_ARGS);
    g_free (gs_path);
    for (i = 0; i < NUM_GS_ARGS && gs_args[i]; i++, argc++) {
        argv[argc] = gs_args[i];
    }
    
    ps2pdf_args = g_strsplit (PS_TO_PDF_PARAMS, " ", NUM_PS2PDF_ARGS);
    for (i = 0; i < NUM_PS2PDF_ARGS && ps2pdf_args[i]; i++, argc++) {
        argv[argc] = ps2pdf_args[i];
    }

    argv[argc++] = "-q";
    argv[argc++] = "-dSAFER";
    argv[argc++] = "-dNOPAUSE";
    argv[argc++] = "-dBATCH";
    argv[argc++] = "-sOutputFile=-"; // stdout
    argv[argc++] = ps2pdf->filename;

    argv[argc++] = NULL;

    if (g_spawn_async_with_pipes (dir, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                                  NULL, NULL,
                                  &(ps2pdf->pid), &pin, &pout, &perr,
                                  &error)) {
        GIOFlags flags;

        ps2pdf->input = g_io_channel_unix_new (pin);
        g_io_channel_set_encoding (ps2pdf->input, NULL, NULL);
        flags = g_io_channel_get_flags (ps2pdf->input);
        g_io_channel_set_flags (ps2pdf->input, flags | G_IO_FLAG_NONBLOCK, NULL);
        ps2pdf->input_id = 0;
        
        ps2pdf->output = g_io_channel_unix_new (pout);
        g_io_channel_set_encoding (ps2pdf->output, NULL, NULL);
        flags = g_io_channel_get_flags (ps2pdf->output);
        g_io_channel_set_flags (ps2pdf->output, flags | G_IO_FLAG_NONBLOCK, NULL);
        ps2pdf->output_id = g_io_add_watch (ps2pdf->output, G_IO_IN,
                                        (GIOFunc)pspdf_converter_output,
                                        ps2pdf);
        
        ps2pdf->error = g_io_channel_unix_new (perr);
        flags = g_io_channel_get_flags (ps2pdf->error);
        g_io_channel_set_flags (ps2pdf->error, flags | G_IO_FLAG_NONBLOCK, NULL);
        ps2pdf->error_id = g_io_add_watch (ps2pdf->error, G_IO_IN,
                                       (GIOFunc)pspdf_converter_error,
                                       ps2pdf);
        
        g_child_watch_add (ps2pdf->pid,
                           (GChildWatchFunc)pspdf_converter_finished, 
                           ps2pdf);
    } else {
        g_warning (error->message);
        g_error_free (error);
    }

    g_free (dir);
    g_strfreev (gs_args);
    g_strfreev (ps2pdf_args);
}

void
pspdf_converter_start_sync (PSPDFConverter *ps2pdf)
{
    gchar *argv[NUM_ARGS], *dir, *gs_path;
    gchar **gs_args, **ps2pdf_args = NULL;
    gint pin, pout, perr;
    gint argc = 0, i;
    GError *error = NULL;

    GIOStatus status;
    gchar *stdoutput;
    gsize stdoutput_length;

    g_assert (ps2pdf->filename != NULL);
    g_assert (ps2pdf->pid == -1); // one converter per object

    pspdf_converter_stop (ps2pdf);

    dir = g_path_get_dirname (ps2pdf->filename);
    gs_path = g_find_program_in_path ("gs");
    gs_args = g_strsplit (gs_path, " ", NUM_GS_ARGS);
    g_free (gs_path);
    for (i = 0; i < NUM_GS_ARGS && gs_args[i]; i++, argc++) {
        argv[argc] = gs_args[i];
    }
    
    ps2pdf_args = g_strsplit (PS_TO_PDF_PARAMS, " ", NUM_PS2PDF_ARGS);
    for (i = 0; i < NUM_PS2PDF_ARGS && ps2pdf_args[i]; i++, argc++) {
        argv[argc] = ps2pdf_args[i];
    }

    argv[argc++] = "-q";
    argv[argc++] = "-dSAFER";
    argv[argc++] = "-dNOPAUSE";
    argv[argc++] = "-dBATCH";
    argv[argc++] = "-sOutputFile=-"; // stdout
    argv[argc++] = ps2pdf->filename;

    argv[argc++] = NULL;

    if (g_spawn_async_with_pipes (dir, argv, NULL, 0,
                                  NULL, NULL,
                                  &(ps2pdf->pid), &pin, &pout, &perr,
                                  &error)) {
        ps2pdf->output = g_io_channel_unix_new (pout);
        g_io_channel_set_encoding (ps2pdf->output, NULL, NULL);
        ps2pdf->output_id = 0;

        status = g_io_channel_read_to_end(ps2pdf->output, &stdoutput,
                &stdoutput_length, &error);
        g_io_channel_unref(ps2pdf->output);
        ps2pdf->output = NULL;

        if (status == G_IO_STATUS_NORMAL) {
            g_string_append_len(ps2pdf->pdfdata, stdoutput, stdoutput_length);
            pspdf_converter_finished(ps2pdf->pid, 0, ps2pdf);
            g_free(stdoutput);
        } else {
            g_warning (error->message);
            g_error_free (error);
        }
    } else {
        g_warning (error->message);
        g_error_free (error);
    }

    g_free (dir);
    g_strfreev (gs_args);
    g_strfreev (ps2pdf_args);

}

void
pspdf_converter_stop (PSPDFConverter *ps2pdf)
{
    if (ps2pdf->pid > 0) {
        gint status = 0;

        kill (ps2pdf->pid, SIGTERM);
        while ((wait (&status) == -1) && (errno == EINTR));
        g_spawn_close_pid (ps2pdf->pid);
        ps2pdf->pid = -1;
    }

    if (ps2pdf->input) {
        g_io_channel_unref (ps2pdf->input);
        ps2pdf->input = NULL;

        if (ps2pdf->input_id > 0) {
            g_source_remove (ps2pdf->input_id);
            ps2pdf->input_id = 0;
        }
    }

    if (ps2pdf->output) {
        g_io_channel_unref (ps2pdf->output);
        ps2pdf->output = NULL;

        if (ps2pdf->output_id > 0) {
            g_source_remove (ps2pdf->output_id);
            ps2pdf->output_id = 0;
        }
    }

    if (ps2pdf->error) {
        g_io_channel_unref (ps2pdf->error);
        ps2pdf->error = NULL;

        if (ps2pdf->error_id > 0) {
            g_source_remove (ps2pdf->error_id);
            ps2pdf->error_id = 0;
        }
    }
}

GString *
pspdf_converter_get_data(PSPDFConverter *ps2pdf)
{
    return ps2pdf->pdfdata;
}
