/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* pspdf-document.c: Implementation of EvDocument for PS
 *
 * Copyright (C) 2007 Ali Sabil <ali.sabil@gmail.com>
 * Copyright (C) 2004, Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "config.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <poppler.h>
#include <poppler-document.h>
#include <poppler-page.h>
#include "pspdf-document.h"
#include "pspdf-converter.h"
#include "ev-document-misc.h"
#include "ev-file-exporter.h"
#include "ev-file-helpers.h"  /* FIXMEgpoo: Check if needed at all */
#include "../pdf/ev-poppler.h"


struct _PSPDFDocumentClass
{
	PdfDocumentClass parent_class;
};

struct _PSPDFDocument
{
	PdfDocument object;

	PSPDFConverter *converter;

	gchar *uri;
};

typedef struct _PSPDFDocumentClass PSPDFDocumentClass;

static void pspdf_document_file_exporter_iface_init (EvFileExporterInterface       *iface);

EV_BACKEND_REGISTER_WITH_CODE (PSPDFDocument, pspdf_document,
     {
      EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_FILE_EXPORTER, pspdf_document_file_exporter_iface_init);
     });


static void
conversion_finished (PSPDFConverter *pspdf_converter,
	       	     PSPDFDocument  *pspdf_document)
{
	GString* data;
	GError * poppler_error = NULL;

	data = pspdf_converter_get_data(pspdf_converter);
	PDF_DOCUMENT(pspdf_document)->document = poppler_document_new_from_data (data->str,
		       data->len,
		       NULL,
		       NULL);
}

static gboolean
pspdf_document_load (EvDocument  *document,
	             const char  *uri,
		     GError     **error)
{
	PSPDFDocument   *pspdf_document = PSPDF_DOCUMENT (document);

	gchar *filename;

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

	pspdf_document->converter = pspdf_converter_new (filename);
	g_signal_connect (G_OBJECT (pspdf_document->converter), "conversion_finished",
			  G_CALLBACK (conversion_finished),
			  pspdf_document);
	pspdf_converter_start_sync(pspdf_document->converter);

	pspdf_document->uri = g_strdup (uri);

	g_warning ("load: %s", uri);

	return TRUE;
}


static gboolean
pspdf_document_save (EvDocument  *document,
  		     const char  *uri,
		     GError     **error)
{
	PSPDFDocument *pspdf_document = PSPDF_DOCUMENT (document);
    
	g_warning ("save: %s", uri);
	return ev_xfer_uri_simple (pspdf_document->uri, uri, error); 
}


static int
pspdf_document_get_n_pages (EvDocument *document)
{
	/* FIXMEgpoo    
	return poppler_document_get_n_pages (PSPDF_DOCUMENT (document)->document);
	*/
	return 0;
}

static void
pspdf_document_get_page_size (EvDocument   *document,
			      int           page,
			      double       *width,
			      double       *height)
{
	PSPDFDocument *pspdf_document = PSPDF_DOCUMENT (document);
	/* FIXMEgpoo
	PopplerPage *poppler_page;

	poppler_page = poppler_document_get_page (pspdf_document->document, page);
	poppler_page_get_size (poppler_page, width, height);
	g_object_unref (poppler_page);
	*/
}


static void
pspdf_document_dispose (GObject *object)
{
	PSPDFDocument *pspdf_document = PSPDF_DOCUMENT(object);

	if (pspdf_document->converter) {
		g_object_unref (pspdf_document->converter);
		pspdf_document->converter = NULL;
	}

	/*
	if (pspdf_document->document) {
		g_object_unref (pspdf_document->document);
		pspdf_document->document = NULL;
	}
	*/

	if (pspdf_document->uri) {
		g_free(pspdf_document->uri);
		pspdf_document->uri = NULL;
	}

	G_OBJECT_CLASS (pspdf_document_parent_class)->dispose (object);
}



static void
pspdf_document_class_init (PSPDFDocumentClass *klass)
{
	GObjectClass    *gobject_class = G_OBJECT_CLASS (klass);
	PdfDocumentClass *ev_document_class = EV_DOCUMENT_CLASS (klass);
	// PdfDocumentClass *pdf_document_class = PDF_DOCUMENT_CLASS (klass);

	gobject_class->dispose = pspdf_document_dispose;

	EV_DOCUMENT_CLASS (ev_document_class)->load = pspdf_document_load;
	EV_DOCUMENT_CLASS (ev_document_class)->save = pspdf_document_save;
	/*
	ev_document_class->get_n_pages = pspdf_document_get_n_pages;
	ev_document_class->get_page_size = pspdf_document_get_page_size;
	ev_document_class->render = pspdf_document_render;
	*/
}


static void
pspdf_document_init (PSPDFDocument *pspdf_document)
{
	pspdf_document->uri = NULL;
	// PDF_DOCUMENT (pspdf_document)->document = NULL;
	pspdf_document->converter = NULL;
}

static void
pspdf_document_file_exporter_iface_init (EvFileExporterInterface *iface)
{
	/*
        iface->begin = dvi_document_file_exporter_begin;
        iface->do_page = dvi_document_file_exporter_do_page;
        iface->end = dvi_document_file_exporter_end;
	iface->get_capabilities = dvi_document_file_exporter_get_capabilities;
	*/
}
