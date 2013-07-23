/* ev-minimal-toolbar.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2013 Aakash Goenka <aakash.goenka@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ev-minimal-toolbar.h"
#include <math.h>

enum
{
	PROP_0,
	PROP_WINDOW
};

struct _EvMinimalToolbarPrivate {
	EvWindow  *window;
};

G_DEFINE_TYPE (EvMinimalToolbar, ev_minimal_toolbar, GTK_TYPE_TOOLBAR)

static void
ev_minimal_toolbar_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	EvMinimalToolbar *ev_minimal_toolbar = EV_MINIMAL_TOOLBAR (object);

	switch (prop_id) {
	case PROP_WINDOW:
		ev_minimal_toolbar->priv->window = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static gint
get_icon_margin (EvMinimalToolbar *ev_minimal_toolbar)
{
	GtkIconSize toolbar_size;
	gint toolbar_size_px, menu_size_px;
	GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (ev_minimal_toolbar));

	toolbar_size = gtk_toolbar_get_icon_size (GTK_TOOLBAR (ev_minimal_toolbar));

	gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &menu_size_px, NULL);
	gtk_icon_size_lookup_for_settings (settings, toolbar_size, &toolbar_size_px, NULL);

	return (gint)floor ((toolbar_size_px - menu_size_px) / 2.0);
}

static void
ev_minimal_toolbar_set_button_image (EvMinimalToolbar *ev_minimal_toolbar,
                                     GtkButton *button)
{
	GtkWidget *image;

	image = gtk_image_new ();
	g_object_set (image, "margin", get_icon_margin (ev_minimal_toolbar), NULL);
	gtk_button_set_image (button, image);
}

static void
ev_minimal_toolbar_set_button_action (EvMinimalToolbar *ev_toolbar,
                                      GtkButton *button,
                                      GtkAction *action)
{
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action);
	gtk_button_set_label (button, NULL);
	gtk_button_set_focus_on_click (button, FALSE);
}

static GtkWidget *
ev_minimal_toolbar_create_button (EvMinimalToolbar *ev_toolbar,
                                  GtkAction *action)
{
	GtkWidget *button = gtk_button_new ();

	ev_minimal_toolbar_set_button_image (ev_toolbar, GTK_BUTTON (button));
	ev_minimal_toolbar_set_button_action (ev_toolbar, GTK_BUTTON (button), action);

	return button;
}

static void
ev_minimal_toolbar_constructed (GObject *object)
{
	EvMinimalToolbar *ev_minimal_toolbar = EV_MINIMAL_TOOLBAR (object);
	GtkActionGroup   *action_group;
	GtkWidget        *tool_item;
	GtkAction        *action;
	GtkWidget        *button;

	G_OBJECT_CLASS (ev_minimal_toolbar_parent_class)->constructed (object);

	/* Set the MENUBAR style class so it's possible to drag the app
	 * using the toolbar. */
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (ev_minimal_toolbar)),
	                             GTK_STYLE_CLASS_MENUBAR);

	action_group = ev_window_get_minimal_toolbar_action_group (ev_minimal_toolbar->priv->window);

	/* Button for file open dialog */
	action = gtk_action_group_get_action (action_group, "ToolbarOpenDocument");
	button = ev_minimal_toolbar_create_button (ev_minimal_toolbar, action);
	tool_item = GTK_WIDGET (gtk_tool_item_new ());
	gtk_container_add (GTK_CONTAINER (tool_item), button);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (ev_minimal_toolbar), tool_item);
	gtk_widget_show (tool_item);

	/* Separator */
	tool_item = GTK_WIDGET (gtk_tool_item_new ());
	gtk_tool_item_set_expand (GTK_TOOL_ITEM (tool_item), TRUE);
	gtk_container_add (GTK_CONTAINER (ev_minimal_toolbar), tool_item);
	gtk_widget_show (tool_item);

	/* About Button */
	action = gtk_action_group_get_action (action_group, "ToolbarAbout");
	button = ev_minimal_toolbar_create_button (ev_minimal_toolbar, action);
	gtk_widget_set_halign (button, GTK_ALIGN_END);
	tool_item = GTK_WIDGET (gtk_tool_item_new ());
	gtk_widget_set_margin_right (tool_item, 6);
	gtk_container_add (GTK_CONTAINER (tool_item), button);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (ev_minimal_toolbar), tool_item);
	gtk_widget_show (tool_item);

	/* Close Button */
	action = gtk_action_group_get_action (action_group, "ToolbarCloseWindow");
	button = ev_minimal_toolbar_create_button (ev_minimal_toolbar, action);
	gtk_widget_set_halign (button, GTK_ALIGN_END);
	tool_item = GTK_WIDGET (gtk_tool_item_new ());
	gtk_container_add (GTK_CONTAINER (tool_item), button);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (ev_minimal_toolbar), tool_item);
	gtk_widget_show (tool_item);
}

static void
ev_minimal_toolbar_class_init (EvMinimalToolbarClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->set_property = ev_minimal_toolbar_set_property;
	g_object_class->constructed = ev_minimal_toolbar_constructed;

	g_object_class_install_property (g_object_class,
	                                 PROP_WINDOW,
	                                 g_param_spec_object ("window",
	                                 "Window",
	                                 "The evince window",
	                                 EV_TYPE_WINDOW,
	                                 G_PARAM_WRITABLE |
	                                 G_PARAM_CONSTRUCT_ONLY |
	                                 G_PARAM_STATIC_STRINGS));

	g_type_class_add_private (g_object_class, sizeof (EvMinimalToolbarPrivate));
}

static void
ev_minimal_toolbar_init (EvMinimalToolbar *ev_minimal_toolbar)
{
	ev_minimal_toolbar->priv = G_TYPE_INSTANCE_GET_PRIVATE (ev_minimal_toolbar, EV_TYPE_MINIMAL_TOOLBAR, EvMinimalToolbarPrivate);
}

GtkWidget *
ev_minimal_toolbar_new (EvWindow *window)
{
	g_return_val_if_fail (EV_IS_WINDOW (window), NULL);

	return GTK_WIDGET (g_object_new (EV_TYPE_MINIMAL_TOOLBAR,
	                                 "window", window,
	                                 NULL));
}
