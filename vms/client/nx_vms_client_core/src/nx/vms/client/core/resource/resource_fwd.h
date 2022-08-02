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

} // namespace nx::vms::client::core

class QnDesktopResource;
using QnDesktopResourcePtr = QnSharedResourcePointer<QnDesktopResource>;
