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

} // namespace nx::vms::client::core

class QnClientStorageResource;
typedef QnSharedResourcePointer<QnClientStorageResource> QnClientStorageResourcePtr;
typedef QnSharedResourcePointerList<QnClientStorageResource> QnClientStorageResourceList;

class QnFakeMediaServerResource;
using QnFakeMediaServerResourcePtr = QnSharedResourcePointer<QnFakeMediaServerResource>;

class QnDesktopResource;
using QnDesktopResourcePtr = QnSharedResourcePointer<QnDesktopResource>;
