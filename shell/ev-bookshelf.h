/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2013 Aakash Goenka
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __EV_BOOKSHELF_H__
#define __EV_BOOKSHELF_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EvBookshelf        EvBookshelf;
typedef struct _EvBookshelfClass   EvBookshelfClass;
typedef struct _EvBookshelfPrivate EvBookshelfPrivate;

#define EV_TYPE_BOOKSHELF              (ev_bookshelf_get_type ())
#define EV_BOOKSHELF(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_BOOKSHELF, EvBookshelf))
#define EV_IS_BOOKSHELF(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_BOOKSHELF))
#define EV_BOOKSHELF_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_BOOKSHELF, EvBookshelfClass))
#define EV_IS_BOOKSHELF_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_BOOKSHELF))
#define EV_BOOKSHELF_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_BOOKSHELF, EvBookshelfClass))

struct _EvBookshelf
{
	GtkScrolledWindow parent;

	EvBookshelfPrivate *priv;
};

struct _EvBookshelfClass
{
	GtkScrolledWindowClass parent_class;

	/* Signals  */
	void (* item_activated) (EvBookshelf *bookshelf,
	                         const char  *uri);
};

GType        ev_bookshelf_get_type          (void) G_GNUC_CONST;
EvBookshelf *ev_bookshelf_new               (void);

G_END_DECLS

#endif /* __EV_BOOKSHELF_H__ */
