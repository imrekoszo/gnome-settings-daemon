/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include <libwacom/libwacom.h>
#include <X11/extensions/XInput.h>

#include "gsd-input-helper.h"

#include "gsd-enums.h"
#include "gsd-wacom-device.h"

#define GSD_WACOM_STYLUS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_WACOM_STYLUS, GsdWacomStylusPrivate))

static WacomDeviceDatabase *db = NULL;

struct GsdWacomStylusPrivate
{
	GsdWacomDevice *device;
	int id;
	char *name;
	const char *icon_name;
	GSettings *settings;
};

static void     gsd_wacom_stylus_class_init  (GsdWacomStylusClass *klass);
static void     gsd_wacom_stylus_init        (GsdWacomStylus      *wacom_stylus);
static void     gsd_wacom_stylus_finalize    (GObject              *object);

G_DEFINE_TYPE (GsdWacomStylus, gsd_wacom_stylus, G_TYPE_OBJECT)

static void
gsd_wacom_stylus_class_init (GsdWacomStylusClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gsd_wacom_stylus_finalize;

        g_type_class_add_private (klass, sizeof (GsdWacomStylusPrivate));
}

static void
gsd_wacom_stylus_init (GsdWacomStylus *stylus)
{
        stylus->priv = GSD_WACOM_STYLUS_GET_PRIVATE (stylus);
}

static void
gsd_wacom_stylus_finalize (GObject *object)
{
        GsdWacomStylus *stylus;
        GsdWacomStylusPrivate *p;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_WACOM_STYLUS (object));

        stylus = GSD_WACOM_STYLUS (object);

        g_return_if_fail (stylus->priv != NULL);

	p = stylus->priv;

        if (p->settings != NULL) {
                g_object_unref (p->settings);
                p->settings = NULL;
        }

        g_free (p->name);
        p->name = NULL;

        G_OBJECT_CLASS (gsd_wacom_stylus_parent_class)->finalize (object);
}

static const char *
get_icon_name_from_type (WacomStylusType type)
{
	switch (type) {
	case WSTYLUS_INKING:
		return "wacom-stylus-inking";
	case WSTYLUS_AIRBRUSH:
		return "wacom-stylus-airbrush";
	default:
		return "wacom-stylus";
	}
}

static GsdWacomStylus *
gsd_wacom_stylus_new (GsdWacomDevice    *device,
		      const WacomStylus *wstylus,
		      GSettings         *settings)
{
	GsdWacomStylus *stylus;

	g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);
	g_return_val_if_fail (wstylus != NULL, NULL);

	stylus = GSD_WACOM_STYLUS (g_object_new (GSD_TYPE_WACOM_STYLUS,
						 NULL));
	stylus->priv->device = device;
	stylus->priv->id = libwacom_stylus_get_id (wstylus);
	stylus->priv->name = g_strdup (libwacom_stylus_get_name (wstylus));
	stylus->priv->settings = settings;
	stylus->priv->icon_name = get_icon_name_from_type (libwacom_stylus_get_type (wstylus));

	return stylus;
}

GSettings *
gsd_wacom_stylus_get_settings (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), NULL);

	return stylus->priv->settings;
}

const char *
gsd_wacom_stylus_get_name (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), NULL);

	return stylus->priv->name;
}

const char *
gsd_wacom_stylus_get_icon_name (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), NULL);

	return stylus->priv->icon_name;
}

GsdWacomDevice *
gsd_wacom_stylus_get_device (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), NULL);

	return stylus->priv->device;
}

#define GSD_WACOM_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_WACOM_DEVICE, GsdWacomDevicePrivate))

/* we support two types of settings:
 * Tablet-wide settings: applied to each tool on the tablet. e.g. rotation
 * Tool-specific settings: applied to one tool only.
 */
#define SETTINGS_WACOM_DIR         "org.gnome.settings-daemon.peripherals.wacom"
#define SETTINGS_STYLUS_DIR        "stylus"
#define SETTINGS_ERASER_DIR        "eraser"

struct GsdWacomDevicePrivate
{
	GdkDevice *gdk_device;
	GsdWacomDeviceType type;
	char *name;
	char *icon_name;
	char *tool_name;
	gboolean reversible;
	gboolean is_screen_tablet;
	GList *styli;
	GsdWacomStylus *last_stylus;
	GSettings *wacom_settings;
};

enum {
	PROP_0,
	PROP_GDK_DEVICE,
	PROP_LAST_STYLUS
};

static void     gsd_wacom_device_class_init  (GsdWacomDeviceClass *klass);
static void     gsd_wacom_device_init        (GsdWacomDevice      *wacom_device);
static void     gsd_wacom_device_finalize    (GObject              *object);

G_DEFINE_TYPE (GsdWacomDevice, gsd_wacom_device, G_TYPE_OBJECT)

static GsdWacomDeviceType
get_device_type (XDeviceInfo *dev)
{
	GsdWacomDeviceType ret;
        static Atom stylus, cursor, eraser, pad, prop;
        XDevice *device;
        Atom realtype;
        int realformat;
        unsigned long nitems, bytes_after;
        unsigned char *data = NULL;
        int rc;

        ret = WACOM_TYPE_INVALID;

        if ((dev->use == IsXPointer) || (dev->use == IsXKeyboard))
                return ret;

        if (!stylus)
                stylus = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "STYLUS", False);
        if (!eraser)
                eraser = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "ERASER", False);
        if (!cursor)
                cursor = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "CURSOR", False);
        if (!pad)
                pad = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "PAD", False);
        /* FIXME: Add touch type? */
        if (!prop)
		prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "Wacom Tool Type", False);

	if (dev->type == stylus)
		ret = WACOM_TYPE_STYLUS;
	if (dev->type == eraser)
		ret = WACOM_TYPE_ERASER;
	if (dev->type == cursor)
		ret = WACOM_TYPE_CURSOR;
	if (dev->type == pad)
		ret = WACOM_TYPE_PAD;

	if (ret == WACOM_TYPE_INVALID)
		return ret;

        /* There is currently no good way of detecting the driver for a device
         * other than checking for a driver-specific property.
         * Wacom Tool Type exists on all tools
         */
        gdk_error_trap_push ();
        device = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), dev->id);
        if (gdk_error_trap_pop () || (device == NULL))
                return ret;

        gdk_error_trap_push ();

        rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                 device, prop, 0, 1, False,
                                 XA_ATOM, &realtype, &realformat, &nitems,
                                 &bytes_after, &data);
        if (gdk_error_trap_pop () || rc != Success || realtype == None) {
                XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
                ret = WACOM_TYPE_INVALID;
        }

        XFree (data);

	return ret;
}

static char *
get_device_name (WacomDevice *device)
{
	return g_strdup_printf ("%s %s",
				libwacom_get_vendor (device),
				libwacom_get_product (device));
}

static GObject *
gsd_wacom_device_constructor (GType                     type,
                              guint                      n_construct_properties,
                              GObjectConstructParam     *construct_properties)
{
        GsdWacomDevice *device;
        XDeviceInfo *device_info;
        WacomDevice *wacom_device;
        int n_devices, id;
        guint i;
        char *path;

        device = GSD_WACOM_DEVICE (G_OBJECT_CLASS (gsd_wacom_device_parent_class)->constructor (type,
												n_construct_properties,
												construct_properties));

	if (device->priv->gdk_device == NULL)
		return G_OBJECT (device);

        g_object_get (device->priv->gdk_device, "device-id", &id, NULL);

        device_info = XListInputDevices (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &n_devices);
        if (device_info == NULL) {
		g_warning ("Could not list any input devices through XListInputDevices()");
		goto end;
	}

        for (i = 0; i < n_devices; i++) {
		if (device_info[i].id == id) {
			device->priv->type = get_device_type (&device_info[i]);
			device->priv->tool_name = g_strdup (device_info[i].name);
			break;
		}
	}

	XFreeDeviceList (device_info);

	if (device->priv->type == WACOM_TYPE_INVALID)
		goto end;

	path = xdevice_get_device_node (id);
	if (path == NULL) {
		g_warning ("Could not get the device node path for ID '%d'", id);
		device->priv->type = WACOM_TYPE_INVALID;
		goto end;
	}

	if (db == NULL)
		db = libwacom_database_new ();

	wacom_device = libwacom_new_from_path (db, path, FALSE, NULL);
	if (!wacom_device) {
		WacomError *wacom_error;

		g_debug ("Creating fallback driver for wacom tablet '%s' (at '%s')",
			 gdk_device_get_name (device->priv->gdk_device),
			 path);

		wacom_error = libwacom_error_new ();
		wacom_device = libwacom_new_from_path (db, path, TRUE, wacom_error);
		if (wacom_device == NULL) {
			g_warning ("Failed to create fallback wacom device for '%s': %s (%d)",
				   path,
				   libwacom_error_get_message (wacom_error),
				   libwacom_error_get_code (wacom_error));
			g_free (path);
			libwacom_error_free (&wacom_error);
			device->priv->type = WACOM_TYPE_INVALID;
			goto end;
		}
	}
	g_free (path);

	/* FIXME
	 * Those should have their own unique path based on a unique property */
	device->priv->wacom_settings = g_settings_new (SETTINGS_WACOM_DIR);

	device->priv->name = get_device_name (wacom_device);
	device->priv->reversible = libwacom_is_reversible (wacom_device);
	device->priv->is_screen_tablet = libwacom_is_builtin (wacom_device);
	if (device->priv->is_screen_tablet) {
		if (libwacom_get_class (wacom_device) == WCLASS_CINTIQ)
			device->priv->icon_name = g_strdup ("wacom-tablet-cintiq");
		else
			device->priv->icon_name = g_strdup ("wacom-tablet-pc");
	} else {
		device->priv->icon_name = g_strdup ("wacom-tablet");
	}

	if (device->priv->type == WACOM_TYPE_STYLUS ||
	    device->priv->type == WACOM_TYPE_ERASER) {
		int *ids;
		int num_styli;
		guint i;

		ids = libwacom_get_supported_styli(wacom_device, &num_styli);
		for (i = 0; i < num_styli; i++) {
			const WacomStylus *wstylus;

			wstylus = libwacom_stylus_get_for_id (db, ids[i]);
			if (wstylus) {
				GsdWacomStylus *stylus;
				GSettings *settings;

				if (device->priv->type == WACOM_TYPE_STYLUS &&
				    libwacom_stylus_is_eraser (wstylus))
					continue;
				if (device->priv->type == WACOM_TYPE_ERASER &&
				    libwacom_stylus_is_eraser (wstylus) == FALSE)
					continue;

				//FIXME settings path!
				if (device->priv->type == WACOM_TYPE_STYLUS) {
					settings = g_settings_new (SETTINGS_WACOM_DIR "." SETTINGS_STYLUS_DIR);
					stylus = gsd_wacom_stylus_new (device, wstylus, settings);
				} else {
					settings = g_settings_new (SETTINGS_WACOM_DIR "." SETTINGS_ERASER_DIR);
					stylus = gsd_wacom_stylus_new (device, wstylus, settings);
				}
				device->priv->styli = g_list_prepend (device->priv->styli, stylus);
			}
		}
		device->priv->styli = g_list_reverse (device->priv->styli);
	}

end:
        return G_OBJECT (device);
}

static void
gsd_wacom_device_set_property (GObject        *object,
                               guint           prop_id,
                               const GValue   *value,
                               GParamSpec     *pspec)
{
        GsdWacomDevice *device;

        device = GSD_WACOM_DEVICE (object);

        switch (prop_id) {
	case PROP_GDK_DEVICE:
		device->priv->gdk_device = g_value_get_pointer (value);
		break;
	case PROP_LAST_STYLUS:
		device->priv->last_stylus = g_value_get_pointer (value);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_wacom_device_get_property (GObject        *object,
                               guint           prop_id,
                               GValue         *value,
                               GParamSpec     *pspec)
{
        GsdWacomDevice *device;

        device = GSD_WACOM_DEVICE (object);

        switch (prop_id) {
	case PROP_GDK_DEVICE:
		g_value_set_pointer (value, device->priv->gdk_device);
		break;
	case PROP_LAST_STYLUS:
		g_value_set_pointer (value, device->priv->last_stylus);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_wacom_device_class_init (GsdWacomDeviceClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gsd_wacom_device_constructor;
        object_class->finalize = gsd_wacom_device_finalize;
        object_class->set_property = gsd_wacom_device_set_property;
        object_class->get_property = gsd_wacom_device_get_property;

        g_type_class_add_private (klass, sizeof (GsdWacomDevicePrivate));

	g_object_class_install_property (object_class, PROP_GDK_DEVICE,
					 g_param_spec_pointer ("gdk-device", "gdk-device", "gdk-device",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_LAST_STYLUS,
					 g_param_spec_pointer ("last-stylus", "last-stylus", "last-stylus",
							       G_PARAM_READWRITE));
}

static void
gsd_wacom_device_init (GsdWacomDevice *device)
{
        device->priv = GSD_WACOM_DEVICE_GET_PRIVATE (device);
        device->priv->type = WACOM_TYPE_INVALID;
}

static void
gsd_wacom_device_finalize (GObject *object)
{
        GsdWacomDevice *device;
        GsdWacomDevicePrivate *p;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_WACOM_DEVICE (object));

        device = GSD_WACOM_DEVICE (object);

        g_return_if_fail (device->priv != NULL);

	p = device->priv;

        if (p->wacom_settings != NULL) {
                g_object_unref (p->wacom_settings);
                p->wacom_settings = NULL;
        }

        g_free (p->name);
        p->name = NULL;

        g_free (p->tool_name);
        p->tool_name = NULL;

        g_free (p->icon_name);
        p->icon_name = NULL;

        G_OBJECT_CLASS (gsd_wacom_device_parent_class)->finalize (object);
}

GsdWacomDevice *
gsd_wacom_device_new (GdkDevice *device)
{
	return GSD_WACOM_DEVICE (g_object_new (GSD_TYPE_WACOM_DEVICE,
					       "gdk-device", device,
					       NULL));
}

GList *
gsd_wacom_device_list_styli (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return g_list_copy (device->priv->styli);
}

const char *
gsd_wacom_device_get_name (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return device->priv->name;
}

const char *
gsd_wacom_device_get_icon_name (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return device->priv->icon_name;
}

const char *
gsd_wacom_device_get_tool_name (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return device->priv->tool_name;
}

gboolean
gsd_wacom_device_reversible (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), FALSE);

	return device->priv->reversible;
}

gboolean
gsd_wacom_device_is_screen_tablet (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), FALSE);

	return device->priv->is_screen_tablet;
}

GSettings *
gsd_wacom_device_get_settings (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return device->priv->wacom_settings;
}

GsdWacomDeviceType
gsd_wacom_device_get_device_type (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), WACOM_TYPE_INVALID);

	return device->priv->type;
}

const char *
gsd_wacom_device_type_to_string (GsdWacomDeviceType type)
{
	switch (type) {
	case WACOM_TYPE_INVALID:
		return "Invalid";
	case WACOM_TYPE_STYLUS:
		return "Stylus";
	case WACOM_TYPE_ERASER:
		return "Eraser";
	case WACOM_TYPE_CURSOR:
		return "Cursor";
	case WACOM_TYPE_PAD:
		return "Pad";
	default:
		return "Unknown type";
	}
}

GsdWacomDevice *
gsd_wacom_device_create_fake (GsdWacomDeviceType  type,
			      const char         *name,
			      const char         *tool_name,
			      gboolean            reversible,
			      gboolean            is_screen_tablet,
			      const char         *icon_name,
			      guint               num_styli)
{
	/* FIXME Look in libwacom's database instead */
#if 0
	GsdWacomDevice *device;
	GsdWacomDevicePrivate *priv;
	guint i;

	device = GSD_WACOM_DEVICE (g_object_new (GSD_TYPE_WACOM_DEVICE, NULL));

	priv = device->priv;
	priv->type = type;
	priv->name = g_strdup (name);
	priv->tool_name = g_strdup (tool_name);
	priv->reversible = reversible;
	priv->is_screen_tablet = is_screen_tablet;
	priv->icon_name = g_strdup (icon_name);
	priv->wacom_settings = g_settings_new (SETTINGS_WACOM_DIR);

	for (i = 0; i < num_styli ; i++) {
		GsdWacomStylus *stylus;
		GSettings *settings;

		if (device->priv->type == WACOM_TYPE_STYLUS) {
			settings = g_settings_new (SETTINGS_WACOM_DIR "." SETTINGS_STYLUS_DIR);
			stylus = gsd_wacom_stylus_new (device, settings, _("Stylus"), "wacom-stylus");
		} else {
			settings = g_settings_new (SETTINGS_WACOM_DIR "." SETTINGS_ERASER_DIR);
			stylus = gsd_wacom_stylus_new (device, settings, "Eraser XXX", NULL);
		}
		device->priv->styli = g_list_append (device->priv->styli, stylus);
	}

	return device;
#endif
	return NULL;
}

GList *
gsd_wacom_device_create_fake_cintiq (void)
{
	GsdWacomDevice *device;
	GList *devices;

	device = gsd_wacom_device_create_fake (WACOM_TYPE_STYLUS,
					       "Wacom Cintiq 21UX2",
					       "Wacom Cintiq 21UX2 stylus",
					       TRUE,
					       TRUE,
					       "wacom-tablet-cintiq",
					       1);
	devices = g_list_prepend (NULL, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_ERASER,
					       "Wacom Cintiq 21UX2",
					       "Wacom Cintiq 21UX2 eraser",
					       TRUE,
					       TRUE,
					       "wacom-tablet-cintiq",
					       1);
	devices = g_list_prepend (devices, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_PAD,
					       "Wacom Cintiq 21UX2",
					       "Wacom Cintiq 21UX2 pad",
					       TRUE,
					       TRUE,
					       "wacom-tablet-cintiq",
					       0);
	devices = g_list_prepend (devices, device);

	return devices;
}

GList *
gsd_wacom_device_create_fake_bt (void)
{
	GsdWacomDevice *device;
	GList *devices;

	device = gsd_wacom_device_create_fake (WACOM_TYPE_STYLUS,
					       "WACOM Pen Tablet",
					       "WACOM Pen Tablet stylus",
					       FALSE,
					       FALSE,
					       "wacom-tablet",
					       1);
	devices = g_list_prepend (NULL, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_ERASER,
					       "WACOM Pen Tablet",
					       "WACOM Pen Tablet eraser",
					       FALSE,
					       FALSE,
					       "wacom-tablet",
					       1);
	devices = g_list_prepend (devices, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_PAD,
					       "WACOM Pen Tablet",
					       "WACOM Pen Tablet pad",
					       FALSE,
					       FALSE,
					       "wacom-tablet",
					       0);
	devices = g_list_prepend (devices, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_CURSOR,
					       "WACOM Pen Tablet",
					       "WACOM Pen Tablet cursor",
					       FALSE,
					       FALSE,
					       "wacom-tablet",
					       0);
	devices = g_list_prepend (devices, device);

	return devices;
}

GList *
gsd_wacom_device_create_fake_x201 (void)
{
	GsdWacomDevice *device;
	GList *devices;

	device = gsd_wacom_device_create_fake (WACOM_TYPE_STYLUS,
					       "Serial Wacom Tablet WACf004",
					       "Serial Wacom Tablet WACf004 stylus",
					       FALSE,
					       TRUE,
					       "wacom-tablet-pc",
					       1);
	devices = g_list_prepend (NULL, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_ERASER,
					       "Serial Wacom Tablet WACf004",
					       "Serial Wacom Tablet WACf004 eraser",
					       FALSE,
					       TRUE,
					       "wacom-tablet-pc",
					       1);
	devices = g_list_prepend (devices, device);

	return devices;
}