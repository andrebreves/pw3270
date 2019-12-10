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
 * Este programa está nomeado como config.c e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 *
 */

 #include <windows.h>

 #include "../pw3270/private.h"

 #include "../common.h"
 #include <stdarg.h>
 #include <glib/gstdio.h>
 #include <v3270/settings.h>

 #ifndef KEY_WOW64_64KEY
	#define KEY_WOW64_64KEY 0x0100
 #endif // KEY_WOW64_64KEY

 #ifndef KEY_WOW64_32KEY
	#define KEY_WOW64_32KEY	0x0200
 #endif // KEY_WOW64_64KEY

/*--[ Globals ]--------------------------------------------------------------------------------------*/

 static GKeyFile	* keyfile 		= NULL;
 static gchar		* keyfilename	= NULL;
 static const HKEY	  predefined[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };

 #define WINDOWS_REGISTRY_PATH "software"

/*--[ Implement ]------------------------------------------------------------------------------------*/

 enum REG_KEY
 {
	REG_KEY_USER,
	REG_KEY_SYSTEM,
	REG_KEY_INEXISTENT
 };

 /// @brief Open registry for read-only access.
 static enum REG_KEY search_for_registry_key(const gchar *group, const gchar *key, HKEY *hKey)
 {
	g_autofree gchar	* path = g_strdup_printf("%s\\%s\\%s",WINDOWS_REGISTRY_PATH,g_get_application_name(),group);
	size_t				  ix;

	for(ix=0;ix < G_N_ELEMENTS(predefined);ix++)
	{
		if(RegOpenKeyEx(predefined[ix],path,0,KEY_READ,hKey) == ERROR_SUCCESS)
		{
			if(RegQueryValueExA(*hKey,key,NULL,NULL,NULL,NULL) == ERROR_SUCCESS)
			{
				trace("Key\"%s\\%s\" found at id %d",path,key,ix);
				return (enum REG_KEY) ix;
			}
			RegCloseKey(*hKey);
		}
	}

	trace("Key \"%s\\%s\""" not found",path,key);

	return REG_KEY_INEXISTENT;
 }

 gboolean pw3270_win32_registry_open(const gchar *group, HKEY *hKey, REGSAM samDesired)
 {
 	size_t				  ix;
	g_autofree gchar	* path = g_strdup_printf("%s\\%s\\%s",WINDOWS_REGISTRY_PATH,g_get_application_name(),group);

	for(ix=0;ix < G_N_ELEMENTS(predefined);ix++)
	{
		if(RegOpenKeyEx(predefined[ix],path,0,samDesired,hKey) == ERROR_SUCCESS)
			return TRUE;
	}

	return FALSE;

 }

 void pw3270_win32_registry_foreach(HKEY parent, const gchar *name,void (*cbk)(const gchar *key, const gchar *val, gpointer *user_data), gpointer *user_data)
 {
	HKEY hKey = 0;

	if(RegOpenKeyEx(parent,name,0,KEY_READ,&hKey) == ERROR_SUCCESS)
	{
		#define MAX_KEY_LENGTH 255
		#define MAX_VALUE_NAME 16383

		TCHAR    	pName[MAX_KEY_LENGTH];
		DWORD 		cName	= MAX_KEY_LENGTH;
		int			ix		= 0;

		while(RegEnumValue(hKey,ix++,pName,&cName,NULL,NULL,NULL,NULL) == ERROR_SUCCESS)
		{
			BYTE data[4097];
			unsigned long datatype;
			unsigned long datalen 	= 4096;

			if(RegQueryValueExA(hKey,pName,NULL,&datatype,data,&datalen) == ERROR_SUCCESS)
			{
				data[datalen+1] = 0;
				cbk(pName,(const gchar *) data, user_data);
			}
			cName = MAX_KEY_LENGTH;
		}
		RegCloseKey(hKey);
	}
 }


 /*
 static BOOL registry_open_key(const gchar *group, const gchar *key, REGSAM samDesired, HKEY *hKey)
 {
 	static HKEY	  predefined[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
 	int			  f;
	gchar		* path = g_strdup_printf("%s\\%s\\%s",registry_path,g_get_application_name(),group);

	for(f=0;f<G_N_ELEMENTS(predefined);f++)
	{
		if(RegOpenKeyEx(predefined[f],path,0,samDesired,hKey) == ERROR_SUCCESS)
		{
			g_free(path);
			return TRUE;
		}
	}

	g_free(path);

	return FALSE;
 }
 */


 gboolean get_boolean_from_config(const gchar *group, const gchar *key, gboolean def)
 {
	HKEY hKey;

	if(keyfile)
	{
		GError		* err = NULL;
		gboolean	  val = g_key_file_get_boolean(keyfile,group,key,&err);
		if(err)
			g_error_free(err);
		else
			return val;
	}
	else if(search_for_registry_key(group,key,&hKey) != REG_KEY_INEXISTENT)
	{
		DWORD			data;
		unsigned long	datalen	= sizeof(data);
		unsigned long	datatype;

		if(RegQueryValueExA(hKey,key,NULL,&datatype,(BYTE *) &data,&datalen) == ERROR_SUCCESS)
		{
			if(datatype == REG_DWORD)
				def = data ? TRUE : FALSE;
			else
				g_warning("Unexpected registry data type in %s\\%s\\%s\\%s",WINDOWS_REGISTRY_PATH,g_get_application_name(),group,key);
		}

		RegCloseKey(hKey);

	}

	return def;
 }

 gint get_integer_from_config(const gchar *group, const gchar *key, gint def)
 {
	HKEY hKey;

	if(keyfile)
	{
		GError	* err = NULL;
		gint	  val = g_key_file_get_integer(keyfile,group,key,&err);
		if(err)
			g_error_free(err);
		else
			return val;
	}
	else if(search_for_registry_key(group,key,&hKey) != REG_KEY_INEXISTENT)
	{
		DWORD			data;
		gint			ret = def;
		unsigned long	datalen	= sizeof(data);
		unsigned long	datatype;

		if(RegQueryValueExA(hKey,key,NULL,&datatype,(BYTE *) &data,&datalen) == ERROR_SUCCESS)
		{
			if(datatype == REG_DWORD)
				def = (gint) data;
			else
				g_warning("Unexpected registry data type in %s\\%s\\%s\\%s",WINDOWS_REGISTRY_PATH,g_get_application_name(),group,key);
		}

		RegCloseKey(hKey);
	}

	return def;
 }


 gchar * get_string_from_config(const gchar *group, const gchar *key, const gchar *def)
 {
	HKEY hKey;

	if(keyfile)
	{
		gchar *ret = g_key_file_get_string(keyfile,group,key,NULL);
		if(ret)
			return ret;
	}
	else if(search_for_registry_key(group,key,&hKey) != REG_KEY_INEXISTENT)
	{
		unsigned long		  datalen 	= 4096;
		g_autofree gchar	* data = g_malloc0(datalen+2);
		unsigned long		  datatype;

		if(RegQueryValueExA(hKey,key,NULL,&datatype,data,&datalen) == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			data[datalen+1] = 0;
			return g_strdup((const gchar *) data);
		}

		RegCloseKey(hKey);
	}

	if(def)
		return g_strdup(def);

	return NULL;

 }

void pw3270_session_config_load(const gchar *filename)
{
	if(keyfilename)
	{
		g_free(keyfilename);
		keyfilename = NULL;
	}

	if(keyfile)
	{
		g_key_file_free(keyfile);
		keyfile = NULL;
	}

	if(filename)
	{
		// Has file name, load it.
		g_message(_("Loading %s"),filename);
		keyfile = g_key_file_new();
		g_key_file_load_from_file(keyfile,filename,G_KEY_FILE_NONE,NULL);
		keyfilename = g_strdup(filename);
	}

}


void set_string_to_config(const gchar *group, const gchar *key, const gchar *fmt, ...)
{
	HKEY hKey;
	g_autofree gchar * str = NULL;

	if(fmt)
	{
		va_list args;
		va_start(args, fmt);
		str = g_strdup_vprintf(fmt,args);
		va_end(args);
	}

	if(keyfile)
	{
		// Update keyfile
		if(str)
		{
			g_key_file_set_string(keyfile,group,key,str);
		}
		else
		{
			g_key_file_remove_key(keyfile,group,key,NULL);
		}

	}


}

void set_boolean_to_config(const gchar *group, const gchar *key, gboolean val)
{
	HKEY hKey;

	if(keyfile)
	{
		g_key_file_set_boolean(keyfile,group,key,val);
	}

}

void set_integer_to_config(const gchar *group, const gchar *key, gint val)
{
	HKEY hKey;

	if(keyfile)
	{
		g_key_file_set_integer(keyfile,group,key,val);
	}

}

void pw3270_session_config_save()
{
	if(!keyfile)
		return;

	g_autofree gchar * text = g_key_file_to_data(keyfile,NULL,NULL);

	if(text)
	{
		GError * error = NULL;

		if(keyfilename)
		{
			g_message( _("Saving %s"), keyfilename);
			g_file_set_contents(keyfilename,text,-1,&error);
		}
		else
		{
			g_autofree gchar * name = g_strconcat(g_get_application_name(),".conf",NULL);
			g_autofree gchar * filename = g_build_filename(g_get_user_config_dir(),name,NULL);
			g_mkdir_with_parents(g_get_user_config_dir(),S_IRUSR|S_IWUSR);
			g_file_set_contents(filename,text,-1,&error);
		}


		if(error) {
			g_message( _( "Can't save session settings: %s" ), error->message);
			g_error_free(error);
		}

	}

}

void pw3270_session_config_free(void)
{
	if(keyfile)
	{
		pw3270_session_config_save();
		g_key_file_free(keyfile);
		keyfile = NULL;
	}
}

gchar * build_data_filename(const gchar *first_element, ...)
{
	va_list args;
	gchar	*path;

	va_start(args, first_element);
	path = filename_from_va(first_element,args);
	va_end(args);
	return path;
}

gchar * filename_from_va(const gchar *first_element, va_list args)
{
	const gchar * appname[]	= { g_get_application_name(), PACKAGE_NAME };
	size_t p,f;

	// Make base path
	const gchar *element;
	GString	* result = g_string_new("");
	for(element = first_element;element;element = va_arg(args, gchar *))
    {
    	g_string_append_c(result,G_DIR_SEPARATOR);
    	g_string_append(result,element);
    }

	g_autofree gchar * suffix = g_string_free(result, FALSE);

	// Check system data dirs
	const gchar * const * system_data_dirs = g_get_system_data_dirs();
	for(p=0;p<G_N_ELEMENTS(appname);p++)
	{
		for(f=0;system_data_dirs[f];f++)
		{
			gchar * path = g_build_filename(system_data_dirs[f],appname[p],suffix,NULL);
			trace("searching \"%s\"",path);
			if(g_file_test(path,G_FILE_TEST_EXISTS))
				return path;
			g_free(path);
		}

	}

	// Check current dir
	g_autofree gchar *dir = g_get_current_dir();
	gchar * path = g_build_filename(dir,suffix,NULL);
	trace("searching \"%s\"",path);
	if(g_file_test(path,G_FILE_TEST_EXISTS))
		return path;
	g_free(path);

	trace("Can't find \"%s\"",suffix);

	return g_build_filename(".",suffix,NULL);
}

GKeyFile * pw3270_session_config_get(gboolean create)
{
	if(!keyfile && create)
		pw3270_session_config_load(NULL);
	return keyfile;
}

 static const struct _WindowState
 {
        const char *name;
        GdkWindowState flag;
        void (*activate)(GtkWindow *);
 } WindowState[] =
 {
        { "Maximized",	GDK_WINDOW_STATE_MAXIMIZED,	gtk_window_maximize             },
        { "Iconified",	GDK_WINDOW_STATE_ICONIFIED,	gtk_window_iconify              },
        { "Sticky",		GDK_WINDOW_STATE_STICKY,	gtk_window_stick                }
 };

 void pw3270_session_save_terminal(GtkWidget *terminal)
 {
	HKEY hKey;

	GKeyFile * keyfile = pw3270_session_config_get(FALSE);

	if(keyfile)
	{
		v3270_to_key_file(terminal, keyfile, "terminal");
		pw3270_session_config_save();
	}


 }

 void save_window_state_to_config(const gchar *group, const gchar *key, GdkWindowState CurrentState)
 {

 }

 void save_window_size_to_config(const gchar *group, const gchar *key, GtkWidget *hwnd)
 {

 }

 void restore_window_from_config(const gchar *group, const gchar *key, GtkWidget *hwnd)
 {

 }

