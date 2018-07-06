#pragma once

#include <core/resource/client_core_resource_fwd.h>

class QnDesktopResource;
using QnDesktopResourcePtr = QnSharedResourcePointer<QnDesktopResource>;

class QnClientCameraResource;
typedef QnSharedResourcePointer<QnClientCameraResource> QnClientCameraResourcePtr;
typedef QnSharedResourcePointerList<QnClientCameraResource> QnClientCameraResourceList;

struct QnLayoutTourItem;
using QnLayoutTourItemList = std::vector<QnLayoutTourItem>;
