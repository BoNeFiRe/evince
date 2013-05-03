/* pdfdocument.h: Implementation of EvDocument for PS
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

#ifndef __PSPDF_DOCUMENT_H__
#define __PSPDF_DOCUMENT_H__

#include "ev-document.h"
#include "../pdf/ev-poppler.h"

G_BEGIN_DECLS

#define PSPDF_TYPE_DOCUMENT             (pspdf_document_get_type ())
#define PSPDF_DOCUMENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PSPDF_TYPE_DOCUMENT, PSPDFDocument))
#define PSPDF_IS_DOCUMENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PSPDF_TYPE_DOCUMENT))

typedef struct _PSPDFDocument PSPDFDocument;

// FIXMEgpoo: Remove the next line if not needed
// PSPDFDocument   *pspdf_document_new     (void);
GType           pspdf_document_get_type (void) G_GNUC_CONST;

G_MODULE_EXPORT GType register_evince_backend (GTypeModule *module);
     
G_END_DECLS

#endif /* __PSPDF_DOCUMENT_H__ */
