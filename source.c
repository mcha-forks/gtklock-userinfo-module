// gtklock-userinfo-module
// Copyright (c) 2022 Erik Reider, Jovan Lanik

// User info module

#include <act/act.h>

#include "gtklock-module.h"

#define MODULE_DATA(x) (x->module_data[self_id])
#define USERINFO(x) ((struct userinfo *)MODULE_DATA(x))

extern void config_load(const char *path, const char *group, GOptionEntry entries[]);

struct userinfo {
	GtkWidget *user_revealer;
	GtkWidget *user_box;
	GtkWidget *user_icon;
	GtkWidget *user_name;
};

static int self_id;

static ActUserManager *act_manager = NULL;
static ActUser *act_user = NULL;

static gboolean round_image = TRUE;
static gboolean vertical_layout = TRUE;
static gboolean under_clock = FALSE;

static GOptionEntry userinfo_entries[] = {
	{ "round-image", 0, 0, G_OPTION_ARG_NONE, &round_image, NULL, NULL },
	{ "vertical-layout", 0, 0, G_OPTION_ARG_NONE, &vertical_layout, NULL, NULL },
	{ "under-clock", 0, 0, G_OPTION_ARG_NONE, &under_clock, NULL, NULL },
	{ NULL },
};

static void window_set_userinfo(ActUser* user, struct Window *ctx) {
	const char *name = act_user_get_real_name(user);
	if(name == NULL) {
		g_warning("User name not found!\n");
		gtk_container_remove(GTK_CONTAINER(ctx->window_box), USERINFO(ctx)->user_name);
	} else gtk_label_set_text(GTK_LABEL(USERINFO(ctx)->user_name), name);

	const char *path = act_user_get_icon_file(user);
	if(path == NULL) {
		g_warning("User image not found!\n");
		gtk_container_remove(GTK_CONTAINER(ctx->window_box), USERINFO(ctx)->user_icon);
		return;
	}
	const int size = 96;
	const int h_size = size / 2;

	GError *error = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(path, size, size, &error);
	if(pixbuf == NULL) {
		g_warning("User image error: %s\n", error->message);
		gtk_container_remove(GTK_CONTAINER(ctx->window_box), USERINFO(ctx)->user_icon);
		return;
	}

	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
	cairo_t *cr = cairo_create(surface);
	gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);

	if(round_image) {
		// Makes the image circular
		cairo_arc(cr, h_size, h_size, h_size, 0, 2 * G_PI);
		cairo_clip(cr);
	}
	cairo_paint(cr);

	gtk_image_set_from_surface(GTK_IMAGE(USERINFO(ctx)->user_icon), surface);
}

static void window_user_loaded(ActUser* user, struct Window *ctx) {
	gboolean loaded = FALSE;
	g_object_get(user, "is-loaded", &loaded, NULL);
	if(loaded == TRUE) window_set_userinfo(user, ctx);
}

static void gtklock_set_userinfo(ActUser* user, struct GtkLock *gtklock) {
	if(gtklock->focused_window != NULL) window_set_userinfo(user, gtklock->focused_window);
	else if(!gtklock->use_layer_shell) window_set_userinfo(user, g_array_index(gtklock->windows, struct Window *, 0));
}

static void gtklock_user_loaded(ActUser* user, GParamSpec *spec, struct GtkLock *gtklock) {
	if(gtklock->focused_window != NULL) window_user_loaded(user, gtklock->focused_window);
	else if(!gtklock->use_layer_shell) window_user_loaded(user, g_array_index(gtklock->windows, struct Window *, 0));
}

static void init_user_manager(struct GtkLock *gtklock) {
	if(act_manager == NULL) act_manager = act_user_manager_get_default();
	if(act_user_manager_no_service(act_manager)) {
		g_warning("AccountsService is not running!\n");
		return;
	}
	if(act_user == NULL) {
		const char *username = g_get_user_name();
		act_user = act_user_manager_get_user(act_manager, username);
		g_signal_connect(act_user, "notify::is-loaded", G_CALLBACK(gtklock_user_loaded), gtklock);
		g_signal_connect(act_user, "changed", G_CALLBACK(gtklock_set_userinfo), gtklock);
	}
}

static void setup_userinfo(struct Window *ctx) {
	if(MODULE_DATA(ctx) != NULL) {
		gtk_widget_destroy(USERINFO(ctx)->user_revealer);
		g_free(MODULE_DATA(ctx));
		MODULE_DATA(ctx) = NULL;
	}
	MODULE_DATA(ctx) = g_malloc(sizeof(struct userinfo));

	USERINFO(ctx)->user_revealer = gtk_revealer_new();
	gtk_widget_set_halign(USERINFO(ctx)->user_revealer, GTK_ALIGN_CENTER);
	gtk_widget_set_name(USERINFO(ctx)->user_revealer, "user-revealer");
	gtk_revealer_set_reveal_child((GtkRevealer *)USERINFO(ctx)->user_revealer, TRUE);
	gtk_revealer_set_transition_type(GTK_REVEALER(USERINFO(ctx)->user_revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
	gtk_container_add(GTK_CONTAINER(ctx->window_box), USERINFO(ctx)->user_revealer);
	gtk_box_reorder_child(GTK_BOX(ctx->window_box), USERINFO(ctx)->user_revealer, under_clock);

	GtkOrientation o = vertical_layout ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
	USERINFO(ctx)->user_box = gtk_box_new(o, 5);
	gtk_widget_set_halign(USERINFO(ctx)->user_box, GTK_ALIGN_CENTER);
	gtk_widget_set_name(USERINFO(ctx)->user_box, "user-box");
	gtk_container_add(GTK_CONTAINER(USERINFO(ctx)->user_revealer), USERINFO(ctx)->user_box);

	// Profile picture
	USERINFO(ctx)->user_icon = gtk_image_new();
	gtk_widget_set_name(USERINFO(ctx)->user_icon, "user-image");
	g_object_set(USERINFO(ctx)->user_icon, "margin-bottom", 10, NULL);
	gtk_container_add(GTK_CONTAINER(USERINFO(ctx)->user_box), USERINFO(ctx)->user_icon);

	// Profile name
	USERINFO(ctx)->user_name = gtk_label_new(NULL);
	gtk_widget_set_name(USERINFO(ctx)->user_name, "user-name");
	g_object_set(USERINFO(ctx)->user_name, "margin-bottom", 10, NULL);
	gtk_container_add(GTK_CONTAINER(USERINFO(ctx)->user_box), USERINFO(ctx)->user_name);

	window_set_userinfo(act_user, ctx);
	gtk_widget_show_all(USERINFO(ctx)->user_revealer);
}

void g_module_unload(GModule *m) {
	g_object_unref(act_user);
	g_object_unref(act_manager);
}

void on_activation(struct GtkLock *gtklock, int id) {
	self_id = id;
	config_load(gtklock->config_path, "userinfo", userinfo_entries);
	init_user_manager(gtklock);

	GtkCssProvider *provider = gtk_css_provider_new();
	GError *err = NULL;
	const char css[] =
		"#user-name {"
		"font-size: 18pt;"
		"}"
		;

	gtk_css_provider_load_from_data(provider, css, -1, &err);
	if(err != NULL) {
		g_warning("Style loading failed: %s", err->message);
		g_error_free(err);
	} else {
		gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
			GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	g_object_unref(provider);
}

void on_focus_change(struct GtkLock *gtklock, struct Window *win, struct Window *old) {
	setup_userinfo(win);
	if(gtklock->hidden)
		gtk_revealer_set_reveal_child(GTK_REVEALER(USERINFO(win)->user_revealer), FALSE);
	if(old != NULL && win != old)
		gtk_revealer_set_reveal_child(GTK_REVEALER(USERINFO(old)->user_revealer), FALSE);
}

void on_window_empty(struct GtkLock *gtklock, struct Window *ctx) {
	if(MODULE_DATA(ctx) != NULL) {
		g_free(MODULE_DATA(ctx));
		MODULE_DATA(ctx) = NULL;
	}
}

void on_idle_hide(struct GtkLock *gtklock) {
	if(gtklock->focused_window) {
		GtkRevealer *revealer = GTK_REVEALER(USERINFO(gtklock->focused_window)->user_revealer);	
		gtk_revealer_set_reveal_child(revealer, FALSE);
	}
}

void on_idle_show(struct GtkLock *gtklock) {
	if(gtklock->focused_window) {
		GtkRevealer *revealer = GTK_REVEALER(USERINFO(gtklock->focused_window)->user_revealer);	
		gtk_revealer_set_reveal_child(revealer, TRUE);
	}
}

