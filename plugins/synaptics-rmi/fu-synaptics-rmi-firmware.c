/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-synaptics-rmi-firmware.h"

struct _FuSynapticsRmiFirmware {
	FuFirmware		 parent_instance;
};

G_DEFINE_TYPE (FuSynapticsRmiFirmware, fu_synaptics_rmi_firmware, FU_TYPE_FIRMWARE)

static gboolean
fu_synaptics_rmi_firmware_parse (FuFirmware *firmware,
				 GBytes *fw,
				 guint64 addr_start,
				 guint64 addr_end,
				 FwupdInstallFlags flags,
				 GError **error)
{
//	FuSynapticsRmiFirmware *self = FU_SYNAPTICS_RMI_FIRMWARE (firmware);
	//FIXME: parse @fw and add FuFirmwareImage's to @firmware
	//FIXME: need to set the image_id to match fu-synaptics-rmi-device.c -- e.g. 'config'
	return TRUE;
}

static void
fu_synaptics_rmi_firmware_init (FuSynapticsRmiFirmware *self)
{
}

static void
fu_synaptics_rmi_firmware_class_init (FuSynapticsRmiFirmwareClass *klass)
{
	FuFirmwareClass *klass_firmware = FU_FIRMWARE_CLASS (klass);
	klass_firmware->parse = fu_synaptics_rmi_firmware_parse;
}

FuFirmware *
fu_synaptics_rmi_firmware_new (void)
{
	return FU_FIRMWARE (g_object_new (FU_TYPE_SYNAPTICS_RMI_FIRMWARE, NULL));
}
