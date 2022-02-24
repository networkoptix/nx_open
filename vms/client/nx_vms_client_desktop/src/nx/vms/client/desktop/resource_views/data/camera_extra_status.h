// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <core/resource/resource_fwd.h>

#include "resource_tree_globals.h"

namespace nx::vms::client::desktop {

using CameraExtraStatusFlag = ResourceTree::CameraExtraStatusFlag;

Q_DECLARE_FLAGS(CameraExtraStatus, CameraExtraStatusFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(CameraExtraStatus)

CameraExtraStatus getCameraExtraStatus(const QnVirtualCameraResourcePtr& camera);

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::CameraExtraStatus)
