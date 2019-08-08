/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <fcntl.h>
#include <string.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>

#include <glib/gstdio.h>

#include "fu-chunk.h"
#include "fu-synaptics-rmi-common.h"
#include "fu-synaptics-rmi-device.h"
#include "fu-synaptics-rmi-firmware.h"

typedef struct
{
	gint			 fd;
	guint8			 bootloader_id[2];
	guint16			 block_size;
	guint16			 block_count_fw;
	guint16			 block_count_cfg;
} FuSynapticsRmiDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (FuSynapticsRmiDevice, fu_synaptics_rmi_device, FU_TYPE_UDEV_DEVICE)

#define GET_PRIVATE(o) (fu_synaptics_rmi_device_get_instance_private (o))

static void
fu_synaptics_rmi_device_to_string (FuDevice *device, GString *str)
{
	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	FuSynapticsRmiDevicePrivate *priv = GET_PRIVATE (self);
	g_string_append (str, "  FuSynapticsRmiDevice:\n");
	g_string_append_printf (str, "    fd:\t\t\t%i\n", priv->fd);
	g_string_append_printf (str, "    bootloader_id:\t\t%02x%02x\n",
				priv->bootloader_id[0], priv->bootloader_id[1]);
	g_string_append_printf (str, "    block_size:\t\t%i\n", priv->block_size);
	g_string_append_printf (str, "    block_count_fw:\t\t%i\n", priv->block_count_fw);
	g_string_append_printf (str, "    block_count_cfg:\t\t%i\n", priv->block_count_cfg);
}

gboolean
fu_synaptics_rmi_device_set_feature (FuSynapticsRmiDevice *self,
				     const guint8 *data,
				     guint datasz,
				     GError **error)
{
	FuSynapticsRmiDevicePrivate *priv = GET_PRIVATE (self);

	/* Set Feature */
	fu_common_dump_raw (G_LOG_DOMAIN, "SetFeature", data, datasz);
	if (ioctl (priv->fd, HIDIOCSFEATURE(datasz), data) < 0) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INTERNAL,
				     "failed to SetFeature");
		return FALSE;
	}
	return TRUE;
}

gboolean
fu_synaptics_rmi_device_get_feature (FuSynapticsRmiDevice *self,
				     guint8 *data,
				     guint datasz,
				     GError **error)
{
	FuSynapticsRmiDevicePrivate *priv = GET_PRIVATE (self);
	if (ioctl (priv->fd, HIDIOCGFEATURE(datasz), data) < 0) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INTERNAL,
				     "failed to GetFeature");
		return FALSE;
	}
	fu_common_dump_raw (G_LOG_DOMAIN, "GetFeature", data, datasz);
	return TRUE;
}

static gboolean
fu_synaptics_rmi_device_setup (FuDevice *device, GError **error)
{
	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	FuSynapticsRmiDevicePrivate *priv = GET_PRIVATE (self);
	g_autofree gchar *bl_ver = NULL;

	//FIXME: get Function34_Query0,1
	priv->bootloader_id[0] = 0xde;
	priv->bootloader_id[1] = 0xad;
	bl_ver = g_strdup_printf ("%u", priv->bootloader_id[1]);
	fu_device_set_version_bootloader (device, bl_ver);

	//FIXME: get Function34:UImode
	if (TRUE) {
		fu_device_remove_flag (device, FWUPD_DEVICE_FLAG_IS_BOOTLOADER);

		//FIXME: get from hardware
		fu_device_set_version (device, "1.2.3", FWUPD_VERSION_FORMAT_TRIPLET);
	}

	//FIXME: get Function34:FlashProgrammingEn
	if (FALSE) {
		fu_device_add_flag (device, FWUPD_DEVICE_FLAG_IS_BOOTLOADER);

		//FIXME: get Function34_Query3,4
		priv->block_size = 0x20;

		//FIXME: get Function34_Query5,6
		priv->block_count_fw = 0x40;

		//FIXME: get Function34_Query7,8
		priv->block_count_cfg = 0x50;
	}

	/* success */
	return TRUE;
}

static gboolean
fu_synaptics_rmi_device_open (FuDevice *device, GError **error)
{
	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	FuSynapticsRmiDevicePrivate *priv = GET_PRIVATE (self);
	GUdevDevice *udev_device = fu_udev_device_get_dev (FU_UDEV_DEVICE (device));

	/* open device */
	priv->fd = g_open (g_udev_device_get_device_file (udev_device), O_RDWR);
	if (priv->fd < 0) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "failed to open %s",
			     g_udev_device_get_device_file (udev_device));
		return FALSE;
	}


	/* success */
	return TRUE;
}

static gboolean
fu_synaptics_rmi_device_close (FuDevice *device, GError **error)
{
	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	FuSynapticsRmiDevicePrivate *priv = GET_PRIVATE (self);
	if (!g_close (priv->fd, error))
		return FALSE;
	priv->fd = 0;
	return TRUE;
}

static gboolean
fu_synaptics_rmi_device_probe (FuUdevDevice *device, GError **error)
{
	return fu_udev_device_set_physical_id (device, "hid", error);
}

static FuFirmware *
fu_synaptics_rmi_device_prepare_firmware (FuDevice *device,
					  GBytes *fw,
					  FwupdInstallFlags flags,
					  GError **error)
{
	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	FuSynapticsRmiDevicePrivate *priv = GET_PRIVATE (self);
	g_autoptr(FuFirmware) firmware = fu_synaptics_rmi_firmware_new ();
	g_autoptr(GBytes) bytes_cfg = NULL;
	g_autoptr(GBytes) bytes_bin = NULL;

	if (!fu_firmware_parse (firmware, fw, flags, error))
		return NULL;

	/* check sizes */
	bytes_bin = fu_firmware_get_image_by_id_bytes (firmware, NULL, error);
	if (bytes_bin == NULL)
		return FALSE;
	if (g_bytes_get_size (bytes_bin) != priv->block_count_fw * priv->block_size) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INVALID_FILE,
			     "file firmware invalid size 0x%04x",
			     (guint) g_bytes_get_size (bytes_bin));
		return FALSE;
	}
	bytes_cfg = fu_firmware_get_image_by_id_bytes (firmware, "config", error);
	if (bytes_cfg == NULL)
		return FALSE;
	if (g_bytes_get_size (bytes_cfg) != priv->block_count_cfg * priv->block_size) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INVALID_FILE,
			     "file config invalid size 0x%04x",
			     (guint) g_bytes_get_size (bytes_cfg));
		return FALSE;
	}

	return g_steal_pointer (&firmware);
}


static gboolean
fu_synaptics_rmi_device_write_block (FuSynapticsRmiDevice *self,
				     guint8 cmd,
				     guint32 idx,
				     guint32 address,
				     const guint8 *data,
				     guint16 datasz,
				     GError **error)
{
//	FuSynapticsRmiDevicePrivate *priv = GET_PRIVATE (self);

	//FIXME: Write @address tp F34_Flash_Data0,1, but after first chunk it auto-increments...
	if (idx == 0) {
		//FIXME
	}
	//FIXME: write @data to F34_Flash_Data2
	//FIXME: write @cmd into F34_Flash_Data3
	//FIXME: wait for ATTN and check success $80
	return TRUE;
}

static gboolean
fu_synaptics_rmi_device_write_firmware (FuDevice *device,
					FuFirmware *firmware,
					FwupdInstallFlags flags,
					GError **error)
{
	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	FuSynapticsRmiDevicePrivate *priv = GET_PRIVATE (self);
	g_autoptr(FuFirmwareImage) img_fw = NULL;
	g_autoptr(GBytes) bytes_bin = NULL;
	g_autoptr(GBytes) bytes_cfg = NULL;
	g_autoptr(GPtrArray) chunks_bin = NULL;
	g_autoptr(GPtrArray) chunks_cfg = NULL;

	/* use the correct image from the firmware */
	img_fw = fu_firmware_get_image_by_id (firmware, NULL, error);
	if (img_fw == NULL)
		return FALSE;
	g_debug ("using image at addr 0x%0x",
		 (guint) fu_firmware_image_get_addr (img_fw));

	/* get both images */
	bytes_bin = fu_firmware_image_get_bytes (img_fw, error);
	if (bytes_bin == NULL)
		return FALSE;
	bytes_cfg = fu_firmware_get_image_by_id_bytes (firmware, "config", error);
	if (bytes_cfg == NULL)
		return FALSE;

	/* build packets */
	chunks_bin = fu_chunk_array_new_from_bytes (bytes_bin,
						    0x00,	/* start addr */
						    0x00,	/* page_sz */
						    priv->block_size);
	chunks_cfg = fu_chunk_array_new_from_bytes (bytes_cfg,
						    0x00,	/* start addr */
						    0x00,	/* page_sz */
						    priv->block_size);

	/* erase all */
	//FIXME: write $3 into F34_Flash_Data3
	//FIXME: wait for ATTN and check success

	/* write each block */
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_WRITE);
	for (guint i = 0; i < chunks_bin->len; i++) {
		FuChunk *chk = g_ptr_array_index (chunks_bin, i);
		if (!fu_synaptics_rmi_device_write_block (self,
							  0x02, /* write firmware */
							  chk->idx,
							  chk->address,
							  chk->data,
							  chk->data_sz,
							  error))
			return FALSE;
		fu_device_set_progress_full (device, (gsize) i,
					     (gsize) chunks_bin->len + chunks_cfg->len);
	}

	/* program the configuration image */
	for (guint i = 0; i < chunks_cfg->len; i++) {
		FuChunk *chk = g_ptr_array_index (chunks_cfg, i);
		if (!fu_synaptics_rmi_device_write_block (self,
							  0x06, /* write firmware */
							  chk->idx,
							  chk->address,
							  chk->data,
							  chk->data_sz,
							  error))
			return FALSE;
		fu_device_set_progress_full (device,
					     (gsize) chunks_bin->len + i,
					     (gsize) chunks_bin->len + chunks_cfg->len);
	}

	//FIXME
	return TRUE;
}

static gboolean
fu_synaptics_rmi_device_detach (FuDevice *device, GError **error)
{
	/* unlock bootloader */
	//FIXME: write priv->bootloader_id into F34_Flash_Data0,1

	/* enable flash programming */
	//FIXME: write $0F into F34_Flash_Data3 3:0

	//FIXME: wait for ATTN, or just usleep...
	//FIXME: read F34_Flash_Data3 and check for $0, $0, $1

	//FIXME: rescan PDT?
	return TRUE;
}

static gboolean
fu_synaptics_rmi_device_attach (FuDevice *device, GError **error)
{
	//FIXME: write reset $01 into F01_Cmd03 bit0
	//FIXME: wait for ATTN, or just usleep...
	//FIXME: rescan PDT?
	return TRUE;
}

static void
fu_synaptics_rmi_device_init (FuSynapticsRmiDevice *self)
{
	//FIXME: do we get a name from HID?
	//fu_device_set_name (FU_DEVICE (self), "Synaptics RMI4 Device");
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_UPDATABLE);
}

static void
fu_synaptics_rmi_device_class_init (FuSynapticsRmiDeviceClass *klass)
{
	FuDeviceClass *klass_device = FU_DEVICE_CLASS (klass);
	FuUdevDeviceClass *klass_device_udev = FU_UDEV_DEVICE_CLASS (klass);
	klass_device->to_string = fu_synaptics_rmi_device_to_string;
	klass_device->open = fu_synaptics_rmi_device_open;
	klass_device->close = fu_synaptics_rmi_device_close;
	klass_device->prepare_firmware = fu_synaptics_rmi_device_prepare_firmware;
	klass_device->write_firmware = fu_synaptics_rmi_device_write_firmware;
	klass_device->attach = fu_synaptics_rmi_device_attach;
	klass_device->detach = fu_synaptics_rmi_device_detach;
	klass_device->setup = fu_synaptics_rmi_device_setup;
	klass_device_udev->probe = fu_synaptics_rmi_device_probe;
}

FuSynapticsRmiDevice *
fu_synaptics_rmi_device_new (FuUdevDevice *device)
{
	FuSynapticsRmiDevice *self = NULL;
	self = g_object_new (FU_TYPE_SYNAPTICS_RMI_DEVICE, NULL);
	fu_device_incorporate (FU_DEVICE (self), FU_DEVICE (device));
	return self;
}
