// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <core/resource/resource_fwd.h>

/**
 * \file
 *
 * This header file contains common typedefs for different resource types.
 *
 * It is to be included from headers that use these typedefs in declarations,
 * but don't need the definitions of the actual resource classes.
 */

namespace nx::vms::client::core {

class Camera;
using CameraPtr = QnSharedResourcePointer<Camera>;
class DesktopResource;
using DesktopResourcePtr = QnSharedResourcePointer<DesktopResource>;
class ServerResource;
using ServerResourcePtr = QnSharedResourcePointer<ServerResource>;
class UserResource;
using UserResourcePtr = QnSharedResourcePointer<UserResource>;

} // namespace nx::vms::client::core

class QnClientStorageResource;
typedef QnSharedResourcePointer<QnClientStorageResource> QnClientStorageResourcePtr;
typedef QnSharedResourcePointerList<QnClientStorageResource> QnClientStorageResourceList;

class QnFakeMediaServerResource;
using QnFakeMediaServerResourcePtr = QnSharedResourcePointer<QnFakeMediaServerResource>;

class QnClientCameraResource;
using QnClientCameraResourcePtr = QnSharedResourcePointer<QnClientCameraResource>;
using QnClientCameraResourceList = QnSharedResourcePointerList<QnClientCameraResource>;

class QnFileLayoutResource;
using QnFileLayoutResourcePtr = QnSharedResourcePointer<QnFileLayoutResource>;
using QnFileLayoutResourceList = QnSharedResourcePointerList<QnFileLayoutResource>;

struct QnLayoutTourItem;
using QnLayoutTourItemList = std::vector<QnLayoutTourItem>;
