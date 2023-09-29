// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

#include <core/resource/shared_resource_pointer.h>
#include <core/resource/shared_resource_pointer_list.h>

namespace nx::vms::client::desktop {

class ServerResource;
using ServerResourcePtr = QnSharedResourcePointer<ServerResource>;
using ServerResourceList = QnSharedResourcePointerList<ServerResource>;

class LayoutResource;
using LayoutResourcePtr = QnSharedResourcePointer<LayoutResource>;
using LayoutResourceList = QnSharedResourcePointerList<LayoutResource>;

class LayoutItemIndex;
using LayoutItemIndexList = QList<LayoutItemIndex>;

class CrossSystemLayoutResource;
using CrossSystemLayoutResourcePtr = QnSharedResourcePointer<CrossSystemLayoutResource>;
using CrossSystemLayoutResourceList = QnSharedResourcePointerList<CrossSystemLayoutResource>;

} // namespace nx::vms::client::desktop
