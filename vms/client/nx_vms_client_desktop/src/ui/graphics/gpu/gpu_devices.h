// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtQuick/QSGRendererInterface>

#include <nx/vms/client/desktop/settings/types/graphics_api.h>

namespace nx::vms::client::desktop::gpu {

struct DeviceInfo
{
    int index = -1;
    QString name;
    QString additionalInfo;
    bool supportsVideoDecode = false;
};

/**
 * Check if a GPU with Vulkan Video support is present.
 */
NX_VMS_CLIENT_DESKTOP_API bool isVulkanVideoSupported();

/**
 * Set env variable for Qt to use the selected GPU. When name is not empty a GPU is selected by
 * name. Otherwise first GPU capable of HW video decoding is selected.
 */
NX_VMS_CLIENT_DESKTOP_API DeviceInfo selectDevice(
    GraphicsApi api,
    const QString& name = {});

} // nx::vms::client::desktop::gpu
