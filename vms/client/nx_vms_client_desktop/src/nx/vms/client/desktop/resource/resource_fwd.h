// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

#include <core/resource/shared_resource_pointer.h>
#include <core/resource/shared_resource_pointer_list.h>

#include <nx/vms/client/core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

class ServerResource;
using ServerResourcePtr = QnSharedResourcePointer<ServerResource>;
using ServerResourceList = QnSharedResourcePointerList<ServerResource>;

using LayoutResource = core::LayoutResource;
using LayoutResourcePtr = core::LayoutResourcePtr;
using LayoutResourceList = core::LayoutResourceList;

class LayoutItemIndex;
using LayoutItemIndexList = QList<LayoutItemIndex>;

class DesktopCameraPreloaderResource;
using DesktopCameraPreloaderResourcePtr = QnSharedResourcePointer<DesktopCameraPreloaderResource>;
using DesktopCameraPreloaderResourceList =
    QnSharedResourcePointerList<DesktopCameraPreloaderResource>;

} // namespace nx::vms::client::desktop
