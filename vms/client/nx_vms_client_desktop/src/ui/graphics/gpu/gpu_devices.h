// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtQuick/QSGRendererInterface>

namespace nx::vms::client::desktop::gpu {

struct DeviceInfo
{
    QString name;
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
NX_VMS_CLIENT_DESKTOP_API void selectDevice(
    QSGRendererInterface::GraphicsApi api,
    const QString& name = {});

} // nx::vms::client::desktop::gpu
