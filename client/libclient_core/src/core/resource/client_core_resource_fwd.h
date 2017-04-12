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

class QnClientStorageResource;
typedef QnSharedResourcePointer<QnClientStorageResource> QnClientStorageResourcePtr;
typedef QnSharedResourcePointerList<QnClientStorageResource> QnClientStorageResourceList;
