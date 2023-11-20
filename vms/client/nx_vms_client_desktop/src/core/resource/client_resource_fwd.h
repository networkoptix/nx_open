// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/resource_fwd.h>

class QnClientStorageResource;
typedef QnSharedResourcePointer<QnClientStorageResource> QnClientStorageResourcePtr;
typedef QnSharedResourcePointerList<QnClientStorageResource> QnClientStorageResourceList;

class QnClientCameraResource;
using QnClientCameraResourcePtr = QnSharedResourcePointer<QnClientCameraResource>;
using QnClientCameraResourceList = QnSharedResourcePointerList<QnClientCameraResource>;

class QnFileLayoutResource;
using QnFileLayoutResourcePtr = QnSharedResourcePointer<QnFileLayoutResource>;
using QnFileLayoutResourceList = QnSharedResourcePointerList<QnFileLayoutResource>;
