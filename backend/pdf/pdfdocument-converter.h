/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Ali Sabil <ali.sabil@gmail.com>
 *  Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
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

#ifndef __PSPDF_CONVERTER_H__
#define __PSPDF_CONVERTER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _PSPDFConverter        PSPDFConverter;
typedef struct _PSPDFConverterClass   PSPDFConverterClass;

#define PSPDF_TYPE_CONVERTER                (pspdf_converter_get_type())
#define PSPDF_CONVERTER(object)             (G_TYPE_CHECK_INSTANCE_CAST((object), PSPDF_TYPE_CONVERTER, PSPDFConverter))
#define PSPDF_CONVERTER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass), PSPDF_TYPE_CONVERTER, PSPDFConverterClass))
#define PSPDF_IS_CONVERTER(object)          (G_TYPE_CHECK_INSTANCE_TYPE((object), PSPDF_TYPE_CONVERTER))
#define PSPDF_IS_CONVERTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE((klass), PSPDF_TYPE_CONVERTER))
#define PSPDF_CONVERTER_GET_CLASS(object)   (G_TYPE_INSTANCE_GET_CLASS((object), PSPDF_TYPE_CONVERTER, PSPDFConverterClass))

GType           pspdf_converter_get_type    (void) G_GNUC_CONST;
PSPDFConverter  *pspdf_converter_new        (const gchar    *filename);
void            pspdf_converter_start       (PSPDFConverter *ps2pdf);
void            pspdf_converter_start_sync  (PSPDFConverter *ps2pdf);
void            pspdf_converter_stop        (PSPDFConverter *ps2pdf);
GString *       pspdf_converter_get_data    (PSPDFConverter *ps2pdf);
G_END_DECLS

#endif /* __PSPDF_CONVERTER_H__ */
