// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/client_core_resource_fwd.h>

class QnClientCameraResource;
using QnClientCameraResourcePtr = QnSharedResourcePointer<QnClientCameraResource>;
using QnClientCameraResourceList = QnSharedResourcePointerList<QnClientCameraResource>;

class QnFileLayoutResource;
using QnFileLayoutResourcePtr = QnSharedResourcePointer<QnFileLayoutResource>;
using QnFileLayoutResourceList = QnSharedResourcePointerList<QnFileLayoutResource>;

struct QnLayoutTourItem;
using QnLayoutTourItemList = std::vector<QnLayoutTourItem>;
