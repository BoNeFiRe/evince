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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "ev-bookshelf.h"
#include "ev-file-helpers.h"
#include "gd-icon-utils.h"
#include "gd-main-view-generic.h"
#include "gd-main-icon-view.h"
#include "ev-document-misc.h"
#include "ev-document-model.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"
#include "ev-metadata.h"

typedef enum {
	EV_BOOKSHELF_JOB_COLUMN = GD_MAIN_COLUMN_LAST,
	EV_BOOKSHELF_THUMBNAILED_COLUMN,
	EV_BOOKSHELF_DOCUMENT_COLUMN,
	EV_BOOKSHELF_METADATA_COLUMN,
	NUM_COLUMNS
} EvBookshelfColumns;

struct _EvBookshelfPrivate {
	GtkWidget         *view;
	GtkListStore      *list_store;
	GtkRecentManager  *recent_manager;
	gchar             *button_press_item_path;
};

enum {
	ITEM_ACTIVATED = 1,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void     thumbnail_job_completed_callback         (EvJobThumbnail  *job,
				                          EvBookshelf     *ev_bookshelf);
static void     document_load_job_completed_callback     (EvJobLoad   *job_load,
				                          EvBookshelf *ev_bookshelf);
static gboolean ev_bookshelf_clear_job                   (GtkTreeModel *model,
                                                          GtkTreePath *path,
                                                          GtkTreeIter *iter,
                                                          gpointer data);
static void     ev_bookshelf_clear_model                 (EvBookshelf *bookshelf);
static void     ev_bookshelf_refresh                     (EvBookshelf *bookshelf);

G_DEFINE_TYPE (EvBookshelf, ev_bookshelf, GTK_TYPE_SCROLLED_WINDOW)

#define ICON_VIEW_SIZE 128
#define MAX_BOOKSHELF_ITEMS 20

static void
ev_bookshelf_dispose (GObject *obj)
{
	EvBookshelf *self = EV_BOOKSHELF (obj);

	if (self->priv->list_store) {
		ev_bookshelf_clear_model (self);
		g_object_unref (self->priv->list_store);
		self->priv->list_store = NULL;
	}

	if (self->priv->recent_manager) {
		g_signal_handlers_disconnect_by_func (self->priv->recent_manager,
		                                      ev_bookshelf_refresh,
		                                      self);
		self->priv->recent_manager = NULL;
	}
	G_OBJECT_CLASS (ev_bookshelf_parent_class)->dispose (obj);
}

static void
ev_bookshelf_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static gboolean
metadata_is_stale (EvMetadata *metadata, GFile *file)
{
	GFileInfo *info = NULL;
	GError    *error = NULL;
	guint64    mtime_metadata;
	guint64    mtime_file;

	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_TIME_MODIFIED,
	                          0,
	                          NULL,
	                          &error);
	if (!info) {
		g_warning ("%s", error->message);
		g_error_free (error);

		return TRUE;
	}

	if (!ev_metadata_get_uint64 (metadata, "mtime", &mtime_metadata))
		return TRUE;

	mtime_file = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

	if (mtime_file != 0 && mtime_metadata >= mtime_file)
		return FALSE;

	return TRUE;
}

static void
save_thumbnail (GdkPixbuf  *pixbuf,
                EvMetadata *metadata)
{
	GFile     *thumbnail_file = NULL;
	GError    *error = NULL;
	gchar     *thumbnail_path = NULL;
	
	thumbnail_file = ev_mkstemp_file ("thumb.XXXXXX", &error);

	if (thumbnail_file) {
		thumbnail_path = g_file_get_path (thumbnail_file);
		g_object_unref (thumbnail_file);
	}

	if (thumbnail_path) {
		gdk_pixbuf_save (pixbuf, thumbnail_path,
				 "png", &error, NULL);
		if (!error)
			ev_metadata_set_string (metadata, "thumbnail-path", thumbnail_path);
		g_free (thumbnail_path);
	}

	if (error)
		g_error_free (error);
}

static void
ev_bookshelf_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static gboolean
ev_bookshelf_clear_job (GtkTreeModel *model,
                        GtkTreePath *path,
                        GtkTreeIter *iter,
                        gpointer data)
{
	EvJob *job;

	gtk_tree_model_get (model, iter, EV_BOOKSHELF_JOB_COLUMN, &job, -1);

	if (job != NULL) {
		ev_job_cancel (job);
		g_signal_handlers_disconnect_by_func (job, thumbnail_job_completed_callback, data);
		g_signal_handlers_disconnect_by_func (job, document_load_job_completed_callback, data);
		g_object_unref (job);
	}

	return FALSE;    
}

static void 
ev_bookshelf_clear_model (EvBookshelf *bookshelf)
{
	EvBookshelfPrivate *priv = bookshelf->priv;

	gtk_tree_model_foreach (GTK_TREE_MODEL (priv->list_store), ev_bookshelf_clear_job, bookshelf);
	gtk_list_store_clear (priv->list_store);
}

static gint
compare_recent_items (GtkRecentInfo *a, GtkRecentInfo *b)
{
	gboolean     has_ev_a, has_ev_b;
	const gchar *evince = g_get_application_name ();

	has_ev_a = gtk_recent_info_has_application (a, evince);
	has_ev_b = gtk_recent_info_has_application (b, evince);
	
	if (has_ev_a && has_ev_b) {
		time_t time_a, time_b;

		time_a = gtk_recent_info_get_modified (a);
		time_b = gtk_recent_info_get_modified (b);

		return (time_b - time_a);
	} else if (has_ev_a) {
		return -1;
	} else if (has_ev_b) {
		return 1;
	}

	return 0;
}

static GdMainViewGeneric *
get_generic (EvBookshelf *self)
{
	if (self->priv->view != NULL)
		return GD_MAIN_VIEW_GENERIC (self->priv->view);

	return NULL;
}

static gboolean
on_button_release_event (GtkWidget *view,
                         GdkEventButton *event,
                         gpointer user_data)
{
	EvBookshelf *self = user_data;
	GdMainViewGeneric *generic = get_generic (self);
	GtkTreePath *path;
	gchar *button_release_item_path;
	gboolean res, same_item = FALSE;
	
	/* eat double/triple click events */
	if (event->type != GDK_BUTTON_RELEASE)
		return TRUE;

	path = gd_main_view_generic_get_path_at_pos (generic, event->x, event->y);

	if (path != NULL) {
		button_release_item_path = gtk_tree_path_to_string (path);
		if (g_strcmp0 (self->priv->button_press_item_path, button_release_item_path) == 0)
			same_item = TRUE;

		g_free (button_release_item_path);
	}

	g_free (self->priv->button_press_item_path);
	self->priv->button_press_item_path = NULL;

	if (!same_item)
		res = FALSE;
	else {
		GtkTreeIter iter;
		gchar *uri;

		if (self->priv->list_store == NULL)
			goto exit;

		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (self->priv->list_store), &iter, path))
			goto exit;

		gtk_tree_model_get (GTK_TREE_MODEL (self->priv->list_store), &iter,
			            GD_MAIN_COLUMN_URI, &uri,
			            -1);
		gtk_list_store_set (self->priv->list_store,
		                    &iter,
		                    GD_MAIN_COLUMN_SELECTED, TRUE,
		                    -1);
		g_signal_emit (self, signals[ITEM_ACTIVATED], 0, uri);
	}
    exit:
	gtk_tree_path_free (path);
	return res;
}

static gboolean
on_button_press_event (GtkWidget *view,
                       GdkEventButton *event,
                       gpointer user_data)
{
	EvBookshelf *self = user_data;
	GdMainViewGeneric *generic = get_generic (self);
	GtkTreePath *path;

	path = gd_main_view_generic_get_path_at_pos (generic, event->x, event->y);

	if (path != NULL)
		self->priv->button_press_item_path = gtk_tree_path_to_string (path);

	gtk_tree_path_free (path);
	
	return FALSE;
}

static void
thumbnail_job_completed_callback (EvJobThumbnail  *job,
                                  EvBookshelf     *ev_bookshelf)
{
	EvBookshelfPrivate *priv = ev_bookshelf->priv;
	GtkTreeIter        *iter;
	GdkPixbuf          *pixbuf;
	EvDocument         *document;
	EvMetadata         *metadata;
	GtkBorder           border;

	border.left = 4;
	border.right = 3; 
	border.top = 3;
	border.bottom = 6;

	pixbuf = ev_document_misc_render_thumbnail_with_frame (GTK_WIDGET (ev_bookshelf), job->thumbnail);

	pixbuf = gd_embed_image_in_frame (pixbuf,
	                                  "resource:///org/gnome/evince/shell/ui/thumbnail-frame.png",
	                                  &border, &border);
	iter = (GtkTreeIter *) g_object_get_data (G_OBJECT (job), "tree_iter");

	gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store),
	                    iter,
	                    EV_BOOKSHELF_DOCUMENT_COLUMN, &document,
	                    EV_BOOKSHELF_METADATA_COLUMN, &metadata,
	                    -1);

	gtk_list_store_set (priv->list_store,
			    iter,
			    GD_MAIN_COLUMN_ICON, pixbuf,
			    EV_BOOKSHELF_THUMBNAILED_COLUMN, TRUE,
			    EV_BOOKSHELF_JOB_COLUMN, NULL,
			    -1);

	if (metadata) {
		save_thumbnail (pixbuf, metadata);
		ev_metadata_set_uint64 (metadata, "mtime", g_get_real_time ());
		g_object_unref (metadata);
	}
        g_object_unref (pixbuf);
}

static void
document_load_job_completed_callback (EvJobLoad   *job_load,
                                      EvBookshelf *ev_bookshelf)
{
	EvBookshelfPrivate *priv = ev_bookshelf->priv;
	GtkTreeIter        *iter;
	EvDocument         *document;
	EvMetadata         *metadata;

	document = job_load->parent.document;
	iter = (GtkTreeIter *) g_object_get_data (G_OBJECT (job_load), "tree_iter");
	gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store),
	                    iter,
	                    EV_BOOKSHELF_METADATA_COLUMN, &metadata,
	                    -1);

	if (document) {
		EvJob           *job_thumbnail;
		EvDocumentModel *model;
		EvDocumentInfo  *info;
		gdouble          height;
		gdouble          width;
		gint             page;
		gdouble          scale;

		model = ev_document_model_new_with_document (document);
		page = ev_document_model_get_page (model);
		info = ev_document_get_info (document);

		ev_document_get_page_size (document, page, &width, &height);

		scale = (gdouble)ICON_VIEW_SIZE / height < (gdouble)ICON_VIEW_SIZE / width ? 
			(gdouble)ICON_VIEW_SIZE / height : (gdouble)ICON_VIEW_SIZE / width;
		job_thumbnail = ev_job_thumbnail_new (document,
		                                      page,
		                                      ev_document_model_get_rotation (model),
		                                      scale);

		ev_job_thumbnail_set_has_frame (EV_JOB_THUMBNAIL (job_thumbnail), FALSE);

		g_object_set_data_full (G_OBJECT (job_thumbnail), "tree_iter",
					gtk_tree_iter_copy (iter),
					(GDestroyNotify) gtk_tree_iter_free);

		g_signal_connect (job_thumbnail, "finished",
		                  G_CALLBACK (thumbnail_job_completed_callback),
		                  ev_bookshelf);

		gtk_list_store_set (priv->list_store,
				    iter,
		                    GD_MAIN_COLUMN_SECONDARY_TEXT, info->author,
				    EV_BOOKSHELF_THUMBNAILED_COLUMN, FALSE,
				    EV_BOOKSHELF_JOB_COLUMN, job_thumbnail,
		                    EV_BOOKSHELF_DOCUMENT_COLUMN, document,
				    -1);

		ev_job_scheduler_push_job (EV_JOB (job_thumbnail), EV_JOB_PRIORITY_HIGH);

		if (metadata) {
			ev_metadata_set_string (metadata, "author", info->author == NULL ? _("") : info->author);
			g_object_unref (metadata);
		}

		g_object_unref (model);
		g_object_unref (job_thumbnail);

	} else {
		gtk_list_store_set (priv->list_store,
				    iter,
				    EV_BOOKSHELF_THUMBNAILED_COLUMN, TRUE,
				    EV_BOOKSHELF_JOB_COLUMN, NULL,
				    -1);
		if (metadata) {
			GdkPixbuf *thumbnail;

			ev_metadata_set_string (metadata, "author", _(""));
			gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store),
				            iter,
				            GD_MAIN_COLUMN_ICON, &thumbnail,
				            -1);

			if (thumbnail)
				save_thumbnail (thumbnail, metadata);

			ev_metadata_set_uint64 (metadata, "mtime", g_get_real_time ());

			g_object_unref (metadata);
			g_object_unref (thumbnail);
		}
	}
}

static void
ev_bookshelf_refresh (EvBookshelf *ev_bookshelf)
{
	GList             *items, *l;
	guint              n_items = 0;
	const gchar       *evince = g_get_application_name ();
	GdMainViewGeneric *generic = get_generic (ev_bookshelf);

	items = gtk_recent_manager_get_items (ev_bookshelf->priv->recent_manager);
	items = g_list_sort (items, (GCompareFunc) compare_recent_items);

	gtk_list_store_clear (ev_bookshelf->priv->list_store);

	for (l = items; l && l->data; l = g_list_next (l)) {
		EvJob         *job_load = NULL;
		EvMetadata    *metadata = NULL;
		GFile         *file;
		const gchar   *name;
		const gchar   *uri;
		gchar         *thumbnail_path;
		gchar         *author;
		GtkRecentInfo *info;
		GdkPixbuf     *thumbnail;
		GtkTreeIter    iter;
		long           access_time;

		info = (GtkRecentInfo *) l->data;

		if (!gtk_recent_info_has_application (info, evince))
			continue;

		name = gtk_recent_info_get_display_name (info);
		uri = gtk_recent_info_get_uri (info);
		file = g_file_new_for_uri (uri);

		if (ev_is_metadata_supported_for_file (file)) {
			
			metadata = ev_metadata_new (file);
			if (metadata_is_stale (metadata, file) || 
			    !ev_metadata_get_string (metadata, "author", &author) ||
			    !ev_metadata_get_string (metadata, "thumbnail-path", &thumbnail_path))
				goto load_document;

			thumbnail = gdk_pixbuf_new_from_file (thumbnail_path, NULL);
			if (!thumbnail)
				goto load_document;
		} else {

		load_document:

			author = _("");
			thumbnail = gtk_recent_info_get_icon (info, ICON_VIEW_SIZE);
			job_load = ev_job_load_new (uri);
			g_signal_connect (job_load, "finished",
				          G_CALLBACK (document_load_job_completed_callback),
				          ev_bookshelf);
		}
		access_time = gtk_recent_info_get_modified (info);

		gtk_list_store_append (ev_bookshelf->priv->list_store, &iter);

		gtk_list_store_set (ev_bookshelf->priv->list_store, &iter,
		                    GD_MAIN_COLUMN_ID, _("id"),
		                    GD_MAIN_COLUMN_URI, uri,
		                    GD_MAIN_COLUMN_PRIMARY_TEXT, name,
		                    GD_MAIN_COLUMN_SECONDARY_TEXT, author,
		                    GD_MAIN_COLUMN_ICON, thumbnail,
		                    GD_MAIN_COLUMN_MTIME, access_time,
		                    GD_MAIN_COLUMN_SELECTED, FALSE,
		                    EV_BOOKSHELF_DOCUMENT_COLUMN, NULL,
		                    EV_BOOKSHELF_JOB_COLUMN, job_load,
		                    EV_BOOKSHELF_THUMBNAILED_COLUMN, FALSE,
		                    EV_BOOKSHELF_METADATA_COLUMN, metadata,
		                    -1);

		if (job_load) {
			
			g_object_set_data_full (G_OBJECT (job_load), "tree_iter",
			                        gtk_tree_iter_copy (&iter),
			                        (GDestroyNotify) gtk_tree_iter_free);

			ev_job_scheduler_push_job (EV_JOB (job_load), EV_JOB_PRIORITY_HIGH);
		}
		if (thumbnail != NULL)
                        g_object_unref (thumbnail);

		if (++n_items == MAX_BOOKSHELF_ITEMS)
			break;
	}

	g_list_foreach (items, (GFunc) gtk_recent_info_unref, NULL);
	g_list_free (items);

	gd_main_view_generic_set_model (generic, GTK_TREE_MODEL (ev_bookshelf->priv->list_store));
}

static void
ev_bookshelf_rebuild (EvBookshelf *ev_bookshelf)
{
	GtkStyleContext *context;

	if (ev_bookshelf->priv->view != NULL)
		gtk_widget_destroy (ev_bookshelf->priv->view);

	ev_bookshelf->priv->view = gd_main_icon_view_new ();

	context = gtk_widget_get_style_context (ev_bookshelf->priv->view);
	gtk_style_context_add_class (context, "content-view");

	gtk_container_add (GTK_CONTAINER (ev_bookshelf), ev_bookshelf->priv->view);

	g_signal_connect (ev_bookshelf->priv->view, "button-press-event",
	                  G_CALLBACK (on_button_press_event), ev_bookshelf);

	g_signal_connect (ev_bookshelf->priv->view, "button-release-event",
	                  G_CALLBACK (on_button_release_event), ev_bookshelf);

	ev_bookshelf_refresh (ev_bookshelf);

	gtk_widget_show_all (GTK_WIDGET (ev_bookshelf));
}

static void
ev_bookshelf_init (EvBookshelf *ev_bookshelf)
{
	ev_bookshelf->priv = G_TYPE_INSTANCE_GET_PRIVATE (ev_bookshelf, EV_TYPE_BOOKSHELF, EvBookshelfPrivate);
	ev_bookshelf->priv->recent_manager = gtk_recent_manager_get_default ();
	ev_bookshelf->priv->list_store = gtk_list_store_new (11,
	                                                     G_TYPE_STRING,
	                                                     G_TYPE_STRING,
	                                                     G_TYPE_STRING,
	                                                     G_TYPE_STRING,
	                                                     GDK_TYPE_PIXBUF,
	                                                     G_TYPE_LONG,
	                                                     G_TYPE_BOOLEAN,
	                                                     EV_TYPE_JOB,
	                                                     G_TYPE_BOOLEAN,
	                                                     EV_TYPE_DOCUMENT,
	                                                     EV_TYPE_METADATA);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (ev_bookshelf->priv->list_store),
	                                      GD_MAIN_COLUMN_MTIME,
	                                      GTK_SORT_DESCENDING);

	gtk_widget_set_hexpand (GTK_WIDGET (ev_bookshelf), TRUE);
	gtk_widget_set_vexpand (GTK_WIDGET (ev_bookshelf), TRUE);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (ev_bookshelf), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ev_bookshelf),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	g_signal_connect_swapped (ev_bookshelf->priv->recent_manager,
	                          "changed",
	                          G_CALLBACK (ev_bookshelf_refresh),
				  ev_bookshelf);
	ev_bookshelf_rebuild (ev_bookshelf);
}

static void
ev_bookshelf_class_init (EvBookshelfClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->get_property = ev_bookshelf_get_property;
	g_object_class->set_property = ev_bookshelf_set_property;
	g_object_class->dispose = ev_bookshelf_dispose;

	/* Signals */
	
	signals[ITEM_ACTIVATED] =
	          g_signal_new ("item-activated",
	                        EV_TYPE_BOOKSHELF,
	                        G_SIGNAL_RUN_LAST,
	                        G_STRUCT_OFFSET (EvBookshelfClass, item_activated),
	                        NULL, NULL,
	                        g_cclosure_marshal_generic,
	                        G_TYPE_NONE, 1,
	                        G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (EvBookshelfPrivate));
}

EvBookshelf *
ev_bookshelf_new (void)
{
	EvBookshelf *bookshelf;
	
	bookshelf = EV_BOOKSHELF (g_object_new (EV_TYPE_BOOKSHELF, NULL));

	return bookshelf;
}
   
