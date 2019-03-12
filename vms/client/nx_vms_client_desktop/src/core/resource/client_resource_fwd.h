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
