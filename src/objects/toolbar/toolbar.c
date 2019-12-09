/*
 * "Software pw3270, desenvolvido com base nos códigos fontes do WC3270  e X3270
 * (Paul Mattes Paul.Mattes@usa.net), de emulação de terminal 3270 para acesso a
 * aplicativos mainframe. Registro no INPI sob o nome G3270.
 *
 * Copyright (C) <2008> <Banco do Brasil S.A.>
 *
 * Este programa é software livre. Você pode redistribuí-lo e/ou modificá-lo sob
 * os termos da GPL v.2 - Licença Pública Geral  GNU,  conforme  publicado  pela
 * Free Software Foundation.
 *
 * Este programa é distribuído na expectativa de  ser  útil,  mas  SEM  QUALQUER
 * GARANTIA; sem mesmo a garantia implícita de COMERCIALIZAÇÃO ou  de  ADEQUAÇÃO
 * A QUALQUER PROPÓSITO EM PARTICULAR. Consulte a Licença Pública Geral GNU para
 * obter mais detalhes.
 *
 * Você deve ter recebido uma cópia da Licença Pública Geral GNU junto com este
 * programa; se não, escreva para a Free Software Foundation, Inc., 51 Franklin
 * St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Este programa está nomeado como - e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 *
 */

 #include "private.h"
 #include <pw3270/application.h>

 #define GTK_TOOLBAR_DEFAULT_STYLE ((GtkToolbarStyle) -1)

 static gboolean popup_context_menu(GtkToolbar *toolbar, gint x, gint y, gint button_number);
 static void finalize(GObject *object);
 static void pw3270_toolbar_toolbar_set_style(GtkToolbar *toolbar, GtkToolbarStyle style);
 static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
 static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

 static const struct icon_size {
	const gchar			* label;
	GtkIconSize			  icon_size;
 } icon_sizes[] = {

	{
		.label = N_( "System default" ),
		.icon_size = GTK_ICON_SIZE_INVALID

	},

	{
		.label = N_( "Small" ),
		.icon_size = GTK_ICON_SIZE_SMALL_TOOLBAR
	},

	{
		.label = N_( "Large" ),
		.icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR
	},
 };

 static const struct toolbar_style {
	const gchar * label;
	GtkToolbarStyle style;
 } styles[] = {

	{
		.label = N_( "System default" ),
		.style = GTK_TOOLBAR_DEFAULT_STYLE
	},

	{
		.label = N_( "Icons only" ),
		.style = GTK_TOOLBAR_ICONS
	},

	{
		.label = N_( "Text only" ),
		.style = GTK_TOOLBAR_TEXT
	},

	{
		.label = N_( "Icons & text" ),
		.style = GTK_TOOLBAR_BOTH
	},
 };

 enum {
	PROP_NONE,
	PROP_ACTION_NAMES,
 };


 struct _pw3270ToolBar {
 	GtkToolbar parent;

 	/// @brief Popup Menu
 	struct {
		GtkWidget * menu;
		GtkWidget * styles[G_N_ELEMENTS(styles)];
		GtkWidget * icon_sizes[G_N_ELEMENTS(icon_sizes)];
 	} popup;

 };

 struct _pw3270ToolBarClass {

	GtkToolbarClass parent_class;


 };

 G_DEFINE_TYPE(pw3270ToolBar, pw3270ToolBar, GTK_TYPE_TOOLBAR);

 static void pw3270ToolBar_class_init(pw3270ToolBarClass *klass) {

	GObjectClass *object_class = G_OBJECT_CLASS(klass);
 	GtkToolbarClass * toolbar = GTK_TOOLBAR_CLASS(klass);

 	toolbar->popup_context_menu = popup_context_menu;

 	G_OBJECT_CLASS(klass)->finalize = finalize;

	object_class->set_property	= set_property;
	object_class->get_property	= get_property;

	g_object_class_install_property(
		object_class,
		PROP_ACTION_NAMES,
		g_param_spec_string ("action-names",
			N_("Action Names"),
			N_("The name of the actions in the toolbar"),
			NULL,
			G_PARAM_READABLE|G_PARAM_WRITABLE)
	);


 }

  void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec G_GNUC_UNUSED(*pspec)) {

	switch (prop_id) {
    case PROP_ACTION_NAMES:
    	g_value_take_string(value,pw3270_toolbar_get_actions(GTK_WIDGET(object)));
		break;

	default:
		g_assert_not_reached ();
	}

 }

 void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec G_GNUC_UNUSED(*pspec)) {

	switch (prop_id)
	{
    case PROP_ACTION_NAMES:
		pw3270_toolbar_set_actions(GTK_WIDGET(object), g_value_get_string(value));
		break;

	default:
		g_assert_not_reached ();
	}

 }


 static void detacher(GtkWidget *attach_widget, GtkMenu G_GNUC_UNUSED(*menu)) {

 	pw3270ToolBar * toolbar = PW3270_TOOLBAR(attach_widget);
 	toolbar->popup.menu = NULL;

 }

 static void set_icon_size(GtkCheckMenuItem *menuitem, GtkWidget *toolbar) {

	if(gtk_check_menu_item_get_active(menuitem)) {
		const struct icon_size * size = g_object_get_data(G_OBJECT(menuitem),"icon_size");
		pw3270_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), size->icon_size);
	}

 }

 static void set_style(GtkCheckMenuItem *menuitem, GtkWidget *toolbar) {

	if(gtk_check_menu_item_get_active(menuitem)) {
		struct toolbar_style * style = g_object_get_data(G_OBJECT(menuitem),"toolbar_style");
		pw3270_toolbar_toolbar_set_style(GTK_TOOLBAR(toolbar), style->style);
	}

 }

 static void pw3270ToolBar_init(pw3270ToolBar *widget) {

	widget->popup.menu = gtk_menu_new();

	// Size options
	{
		size_t ix;

		GtkWidget * item = gtk_menu_item_new_with_mnemonic( _("Icon _size") );
		gtk_menu_shell_append(GTK_MENU_SHELL(widget->popup.menu),item);

		GtkWidget * submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),submenu);

		for(ix = 0; ix < G_N_ELEMENTS(icon_sizes); ix++) {

			widget->popup.icon_sizes[ix] = item = gtk_check_menu_item_new_with_mnemonic(gettext(icon_sizes[ix].label));
			gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(item),TRUE);

			g_object_set_data(G_OBJECT(item),"icon_size", (gpointer) &icon_sizes[ix]);
			g_signal_connect(item, "toggled", G_CALLBACK(set_icon_size), widget);

			gtk_menu_shell_append(GTK_MENU_SHELL(submenu),item);

		}

	}

	// Style option
	{
		size_t ix;

		GtkWidget * item = gtk_menu_item_new_with_mnemonic( _("S_tyle") );
		gtk_menu_shell_append(GTK_MENU_SHELL(widget->popup.menu),item);

		GtkWidget * submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),submenu);

		for(ix = 0; ix < G_N_ELEMENTS(styles); ix++) {

			widget->popup.styles[ix] = item = gtk_check_menu_item_new_with_mnemonic(gettext(styles[ix].label));
			gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(item),TRUE);

			g_object_set_data(G_OBJECT(item),"toolbar_style", (gpointer) &styles[ix]);
			g_signal_connect(item, "toggled", G_CALLBACK(set_style), widget);

			gtk_menu_shell_append(GTK_MENU_SHELL(submenu),item);

		}

	}


	// gtk_container_set_border_width(GTK_CONTAINER(widget->popup_menu),6);
	gtk_widget_show_all(widget->popup.menu);
	gtk_menu_attach_to_widget(GTK_MENU(widget->popup.menu),GTK_WIDGET(widget),detacher);

	// Bind settings
	GSettings *settings = pw3270_application_get_settings(g_application_get_default());

	if(settings) {
		pw3270_toolbar_toolbar_set_style(GTK_TOOLBAR(widget),g_settings_get_int(settings,"toolbar-style"));
		pw3270_toolbar_set_icon_size(GTK_TOOLBAR(widget),g_settings_get_int(settings,"toolbar-icon-size"));
	}

 }

 static void finalize(GObject *object) {

// 	pw3270ToolBar * toolbar = PW3270_TOOLBAR(object);


	G_OBJECT_CLASS(pw3270ToolBar_parent_class)->finalize(object);

 }

 GtkWidget * pw3270_toolbar_new(void) {
	return g_object_new(PW3270_TYPE_TOOLBAR, NULL);
 }

 GtkWidget * pw3270_toolbar_insert_lib3270_action(GtkWidget *toolbar, const LIB3270_ACTION *action, gint pos) {

	g_return_val_if_fail(GTK_IS_TOOLBAR(toolbar),NULL);

	if(!action) {
		g_message(_("Invalid action identifier"));
		return NULL;
	}

	if(!action->icon) {
		g_message(_("Action \"%s\" doesn't have an icon"), action->name);
		return NULL;
	}

	if(!action->label) {
		g_message(_("Action \"%s\" doesn't have a label"), action->name);
		return NULL;
	}

	debug("Action: %s icon: %s", action->name, action->icon);

	GtkToolItem * item = gtk_tool_button_new(gtk_image_new_from_icon_name(action->icon,GTK_ICON_SIZE_LARGE_TOOLBAR),gettext(action->label));
	gtk_tool_button_set_use_underline(GTK_TOOL_BUTTON(item),TRUE);

	gtk_widget_set_name(GTK_WIDGET(item), action->name);

	if(action->summary)
		gtk_tool_item_set_tooltip_text(item,gettext(action->summary));

	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, pos);

	return GTK_WIDGET(item);
 }

 gboolean popup_context_menu(GtkToolbar *widget, gint G_GNUC_UNUSED(x), gint G_GNUC_UNUSED(y), gint button_number) {

 	pw3270ToolBar * toolbar = PW3270_TOOLBAR(widget);

	debug("%s button_number=%d",__FUNCTION__,button_number);

	if(toolbar->popup.menu) {
		gtk_menu_popup_at_pointer(GTK_MENU(toolbar->popup.menu),NULL);
	}

	return TRUE;

 }

 void pw3270_toolbar_toolbar_set_style(GtkToolbar *toolbar, GtkToolbarStyle style) {

	debug("%s(%d)",__FUNCTION__,(int) style);

	if(style == GTK_TOOLBAR_DEFAULT_STYLE)
		gtk_toolbar_unset_style(GTK_TOOLBAR(toolbar));
	else
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar),style);

	// Store value
	pw3270_settings_set_int("toolbar-style",(int) style);

	// Update menu
	pw3270ToolBar * tb = PW3270_TOOLBAR(toolbar);
	if(tb && tb->popup.menu) {
		size_t ix;
		for(ix = 0; ix < G_N_ELEMENTS(styles); ix++) {

			gtk_check_menu_item_set_active(
				GTK_CHECK_MENU_ITEM(tb->popup.styles[ix]),
				styles[ix].style == style
			);
		}
	}

 }

 void pw3270_toolbar_set_icon_size(GtkToolbar *toolbar, GtkIconSize icon_size) {

	debug("%s(%d)",__FUNCTION__,(int) icon_size);

	if(icon_size == GTK_ICON_SIZE_INVALID)
		gtk_toolbar_unset_icon_size(GTK_TOOLBAR(toolbar));
	else
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),icon_size);

	// Store value
	pw3270_settings_set_int("toolbar-icon-size", (gint) icon_size);

	// Update menu
	pw3270ToolBar * tb = PW3270_TOOLBAR(toolbar);
	if(tb && tb->popup.menu) {
		size_t ix;
		for(ix = 0; ix < G_N_ELEMENTS(icon_sizes); ix++) {

			gtk_check_menu_item_set_active(
				GTK_CHECK_MENU_ITEM(tb->popup.icon_sizes[ix]),
				icon_sizes[ix].icon_size == icon_size
			);
		}
	}

	// gtk_container_foreach(GTK_CONTAINER(toolbar),(GtkCallback) update_child, toolbar);

 }

 void pw3270_toolbar_clear(GtkWidget *toolbar) {

 	GList * children = gtk_container_get_children(GTK_CONTAINER(toolbar));
 	GList * item;

 	for(item = children;item;item = g_list_next(item)) {
		gtk_container_remove(GTK_CONTAINER(toolbar),GTK_WIDGET(item->data));
 	}

 	g_list_free(children);
 }

 void pw3270_toolbar_set_actions(GtkWidget *toolbar, const gchar *action_names) {

 	gchar ** actions = g_strsplit(action_names,",",-1);
 	size_t ix;

 	pw3270_toolbar_clear(toolbar);

	for(ix = 0; actions[ix]; ix++) {
		pw3270_toolbar_insert_action(toolbar,actions[ix],-1);
	}

 	g_strfreev(actions);

 }

 gchar * pw3270_toolbar_get_actions(GtkWidget *toolbar) {

	GString * str = g_string_new("");

 	GList * children = gtk_container_get_children(GTK_CONTAINER(toolbar));
 	GList * item;

 	for(item = children;item;item = g_list_next(item)) {

		if(*str->str)
			g_string_append(str,",");

		if(GTK_IS_SEPARATOR_TOOL_ITEM(item->data)) {
			g_string_append(str,"separator");
		} else if(GTK_IS_TOOL_BUTTON(item->data)) {
			g_string_append(str,gtk_actionable_get_action_name(GTK_ACTIONABLE(item->data)));
		}

 	}

 	g_list_free(children);


	return g_string_free(str,FALSE);
 }
