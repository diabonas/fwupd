/*
 * Copyright (C) 2017 Mario Limonciello <mario.limonciello@dell.com>
 * Copyright (C) 2017 Peichen Huang <peichenhuang@tw.synaptics.com>
 * Copyright (C) 2017-2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-plugin-vfuncs.h"

#include "fu-synapticsmst-common.h"
#include "fu-synapticsmst-device.h"

struct FuPluginData {
	GPtrArray		*devices;
};

/* see https://github.com/hughsie/fwupd/issues/1121 for more details */
static gboolean
fu_synapticsmst_check_amdgpu_safe (GError **error)
{
	gsize bufsz = 0;
	g_autofree gchar *buf = NULL;
	g_auto(GStrv) lines = NULL;

	if (!g_file_get_contents ("/proc/modules", &buf, &bufsz, error))
		return FALSE;

	lines = g_strsplit (buf, "\n", -1);
	for (guint i = 0; lines[i] != NULL; i++) {
		if (g_str_has_prefix (lines[i], "amdgpu ")) {
			g_set_error_literal (error,
					     FWUPD_ERROR,
					     FWUPD_ERROR_INTERNAL,
					     "amdgpu has known issues with synapticsmst");
			return FALSE;
		}
	}

	return TRUE;
}

gboolean
fu_plugin_udev_device_added (FuPlugin *plugin, FuUdevDevice *device, GError **error)
{
	FuPluginData *priv = fu_plugin_get_data (plugin);
	g_autoptr(FuDeviceLocker) locker = NULL;
	g_autoptr(FuSynapticsmstDevice) dev = NULL;
	g_autoptr(GError) error_local = NULL;

	/* interesting device? */
	if (g_strcmp0 (fu_udev_device_get_subsystem (device), "drm_dp_aux_dev") != 0)
		return TRUE;

	dev = fu_synapticsmst_device_new (device);
	locker = fu_device_locker_new (dev, error);
	if (locker == NULL)
		return FALSE;

	/* this might fail if there is nothing connected */
	if (!fu_device_rescan (FU_DEVICE (dev), &error_local)) {
		g_debug ("ignoring failure to rescan: %s", error_local->message);
	} else {
		/* inhibit the idle sleep of the daemon */
		fu_plugin_add_rule (plugin, FU_PLUGIN_RULE_INHIBITS_IDLE,
				    "SynapticsMST can cause the screen to flash when probing");
	}

	/* add */
	fu_plugin_device_add (plugin, FU_DEVICE (dev));
	g_ptr_array_add (priv->devices, g_steal_pointer (&dev));
	return TRUE;
}

gboolean
fu_plugin_startup (FuPlugin *plugin, GError **error)
{
	return fu_synapticsmst_check_amdgpu_safe (error);
}

gboolean
fu_plugin_update (FuPlugin *plugin,
		  FuDevice *device,
		  GBytes *blob_fw,
		  FwupdInstallFlags flags,
		  GError **error)
{
	g_autoptr(FuDeviceLocker) locker = NULL;

	/* sleep to allow device wakeup to complete */
	g_debug ("waiting %d seconds for MST hub wakeup", SYNAPTICS_FLASH_MODE_DELAY);
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_BUSY);
	g_usleep (SYNAPTICS_FLASH_MODE_DELAY * 1000000);

	/* open fd */
	locker = fu_device_locker_new (device, error);
	if (locker == NULL)
		return FALSE;
	return fu_device_write_firmware (device, blob_fw, flags, error);
}

gboolean
fu_plugin_device_removed (FuPlugin *plugin, FuDevice *device, GError **error)
{
	FuPluginData *priv = fu_plugin_get_data (plugin);
	g_ptr_array_remove (priv->devices, device);
	return TRUE;
}

gboolean
fu_plugin_recoldplug (FuPlugin *plugin, GError **error)
{
	FuPluginData *priv = fu_plugin_get_data (plugin);

	/* reprobe all existing devices added by this plugin */
	for (guint i = 0; i < priv->devices->len; i++) {
		FuDevice *device = FU_DEVICE (g_ptr_array_index (priv->devices, i));
		g_autoptr(GError) error_local = NULL;
		if (!fu_device_rescan (device, &error_local)) {
			g_debug ("failed to replug %s: %s",
				 fu_device_get_id (device),
				 error_local->message);
		}
	}
	return TRUE;
}

void
fu_plugin_init (FuPlugin *plugin)
{
	FuPluginData *priv = fu_plugin_alloc_data (plugin, sizeof (FuPluginData));

	/* devices added by this plugin */
	priv->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* make sure dell is already coldplugged */
	fu_plugin_add_rule (plugin, FU_PLUGIN_RULE_RUN_AFTER, "dell");
	fu_plugin_set_build_hash (plugin, FU_BUILD_HASH);
	fu_plugin_add_udev_subsystem (plugin, "drm_dp_aux_dev");
	fu_plugin_add_rule (plugin, FU_PLUGIN_RULE_SUPPORTS_PROTOCOL, "com.synaptics.mst");
}

void
fu_plugin_destroy (FuPlugin *plugin)
{
	FuPluginData *priv = fu_plugin_get_data (plugin);
	g_ptr_array_unref (priv->devices);
}
