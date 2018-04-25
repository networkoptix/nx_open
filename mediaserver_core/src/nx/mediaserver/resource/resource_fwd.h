#pragma once

#include <core/resource/shared_resource_pointer.h>

class QnActiResource;
using QnActiResourcePtr = QnSharedResourcePointer<QnActiResource>;

class QnAdamResource;
using QnAdamResourcePtr = QnSharedResourcePointer<QnAdamResource>;

class QnArchiveCamResource;
using QnArchiveCamResourcePtr = QnSharedResourcePointer<QnArchiveCamResource>;

class QnDesktopCameraResource;
using QnDesktopCameraResourcePtr = QnSharedResourcePointer<QnDesktopCameraResource>;

class QnDigitalWatchdogResource;
using QnDigitalWatchdogResourcePtr = QnSharedResourcePointer<QnDigitalWatchdogResource>;

class QnFlirEIPResource;
using QnFlirEIPResourcePtr = QnSharedResourcePointer<QnFlirEIPResource>;

class QnTestCameraResource;
using QnTestCameraResourcePtr = QnSharedResourcePointer<QnTestCameraResource>;

class QnThirdPartyResource;
using QnThirdPartyResourcePtr = QnSharedResourcePointer<QnThirdPartyResource>;

class QnVistaResource;
using QnVistaResourcePtr = QnSharedResourcePointer<QnVistaResource>;

class QnPlAreconVisionResource;
using QnPlAreconVisionResourcePtr = QnSharedResourcePointer<QnPlAreconVisionResource>;

class QnPlAxisResource;
using QnPlAxisResourcePtr = QnSharedResourcePointer<QnPlAxisResource> ;

class QnPlDlinkResource;
using QnPlDlinkResourcePtr = QnSharedResourcePointer<QnPlDlinkResource>;

class QnPlIsdResource;
using QnPlIsdResourcePtr = QnSharedResourcePointer<QnPlIsdResource>;

class QnPlIqResource;
using QnPlIqResourcePtr = QnSharedResourcePointer<QnPlIqResource>;

class QnPlOnvifResource;
using QnPlOnvifResourcePtr = QnSharedResourcePointer<QnPlOnvifResource>;

class QnPlVmax480Resource;
using QnPlVmax480ResourcePtr = QnSharedResourcePointer<QnPlVmax480Resource>;

namespace nx {
namespace mediaserver {
namespace resource {

class Camera;
using CameraPtr = QnSharedResourcePointer<Camera>;

} // namespace resource
} // namespace mediaserver
} // namespace nx

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HikvisionResource;
using HikvisionResourcePtr = QnSharedResourcePointer<HikvisionResource>;

class LilinResource;
using LilinResourcePtr = QnSharedResourcePointer<LilinResource>;

class HanwhaResource;
using HanwhaResourcePtr = QnSharedResourcePointer<HanwhaResource>;

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

namespace nx {
namespace plugins {
namespace flir {

class FcResource;
using QnFlirFcResourcePtr = QnSharedResourcePointer<FcResource>;

class OnvifResource;
using QnFlirOnvifResourcePtr = QnSharedResourcePointer<OnvifResource> ;

} // namespace flir
} // namespace plugins
} // namespace nx
