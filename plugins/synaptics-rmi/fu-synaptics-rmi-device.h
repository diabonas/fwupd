/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "fu-synaptics-rmi-common.h"
#include "fu-udev-device.h"

G_BEGIN_DECLS

#define FU_TYPE_SYNAPTICS_RMI_DEVICE (fu_synaptics_rmi_device_get_type ())
G_DECLARE_DERIVABLE_TYPE (FuSynapticsRmiDevice, fu_synaptics_rmi_device, FU, SYNAPTICS_RMI_DEVICE, FuUdevDevice)

struct _FuSynapticsRmiDeviceClass
{
	FuUdevDeviceClass	parent_class;
};

//FIXME: if these are not used from a subclassed device they can be removed
gboolean	 fu_synaptics_rmi_device_set_feature	(FuSynapticsRmiDevice	*self,
							 const guint8	*data,
							 guint		 datasz,
							 GError		**error);
gboolean	 fu_synaptics_rmi_device_get_feature	(FuSynapticsRmiDevice	*self,
							 guint8		*data,
							 guint		 datasz,
							 GError		**error);

FuSynapticsRmiDevice	*fu_synaptics_rmi_device_new	(FuUdevDevice	*device);

G_END_DECLS
