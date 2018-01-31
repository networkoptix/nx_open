#pragma once

#include <memory>

#include <licensing/license_fwd.h>

#include "shared_resource_pointer.h"
#include "shared_resource_pointer_list.h"

/**
 * \file
 *
 * This header file contains common typedefs for different resource types.
 *
 * It is to be included from headers that use these typedefs in declarations,
 * but don't need the definitions of the actual resource classes.
 */

// TODO: #gdm move out!
// <--
class QnScheduleTask;

struct QnCameraHistoryItem;

class QnCameraHistory;
typedef QSharedPointer<QnCameraHistory> QnCameraHistoryPtr;
typedef QList<QnCameraHistoryPtr> QnCameraHistoryList;

class QnVideoWallItem;
class QnVideoWallMatrix;
class QnVideoWallControlMessage;

struct QnLayoutItemData;
struct QnLayoutItemResourceDescriptor;

struct QnServerBackupSchedule;

class QnResourceCommand;
typedef std::shared_ptr<QnResourceCommand> QnResourceCommandPtr;

struct QnCameraAdvancedParamValue;
class QnCameraAdvancedParamValueMap;
typedef QList<QnCameraAdvancedParamValue> QnCameraAdvancedParamValueList;

class QnMediaServerConnection;
typedef QSharedPointer<QnMediaServerConnection> QnMediaServerConnectionPtr;

// -->


class QnResourceFactory;
class QnResourcePool;

class QnResource;
typedef QnSharedResourcePointer<QnResource> QnResourcePtr;
typedef QnSharedResourcePointerList<QnResource> QnResourceList;

class QnResourceType;
typedef QSharedPointer<QnResourceType> QnResourceTypePtr;
typedef QList<QnResourceTypePtr> QnResourceTypeList;

class QnVirtualCameraResource;
typedef QnSharedResourcePointer<QnVirtualCameraResource> QnVirtualCameraResourcePtr;
typedef QnSharedResourcePointerList<QnVirtualCameraResource> QnVirtualCameraResourceList;
typedef QSet<QnVirtualCameraResourcePtr> QnVirtualCameraResourceSet;

class QnDesktopCameraResource;
typedef QnSharedResourcePointer<QnDesktopCameraResource> QnDesktopCameraResourcePtr;
typedef QnSharedResourcePointerList<QnDesktopCameraResource> QnDesktopCameraResourceList;

class QnLayoutResource;
typedef QnSharedResourcePointer<QnLayoutResource> QnLayoutResourcePtr;
typedef QnSharedResourcePointerList<QnLayoutResource> QnLayoutResourceList;
typedef QSet<QnLayoutResourcePtr> QnLayoutResourceSet;

class QnLayoutItemIndex;
typedef QList<QnLayoutItemIndex> QnLayoutItemIndexList;

class QnMediaResource;
typedef QSharedPointer<QnMediaResource> QnMediaResourcePtr;
//typedef QnSharedResourcePointerList<QnMediaResource> QnMediaResourceList;

class QnAbstractArchiveResource;
typedef QnSharedResourcePointer<QnAbstractArchiveResource> QnAbstractArchiveResourcePtr;
typedef QnSharedResourcePointerList<QnAbstractArchiveResource> QnAbstractArchiveResourceList;

class QnNetworkResource;
typedef QnSharedResourcePointer<QnNetworkResource> QnNetworkResourcePtr;
typedef QnSharedResourcePointerList<QnNetworkResource> QnNetworkResourceList;

class QnSecurityCamResource;
typedef QnSharedResourcePointer<QnSecurityCamResource> QnSecurityCamResourcePtr;
typedef QnSharedResourcePointer<const QnSecurityCamResource> QnConstSecurityCamResourcePtr;
typedef QnSharedResourcePointerList<QnSecurityCamResource> QnSecurityCamResourceList;

class QnCameraUserAttributes;
typedef QSharedPointer<QnCameraUserAttributes> QnCameraUserAttributesPtr;
typedef QList<QnCameraUserAttributesPtr> QnCameraUserAttributesList;

class QnStorageResource;
typedef QnSharedResourcePointer<QnStorageResource> QnStorageResourcePtr;
typedef QnSharedResourcePointerList<QnStorageResource> QnStorageResourceList;

//class QnAbstractStorageResource;
//typedef QnSharedResourcePointer<QnAbstractStorageResource> QnAbstractStorageResourcePtr;
//typedef QnSharedResourcePointerList<QnAbstractStorageResource> QnAbstractStorageResourceList;

class QnUserResource;
typedef QnSharedResourcePointer<QnUserResource> QnUserResourcePtr;
typedef QnSharedResourcePointerList<QnUserResource> QnUserResourceList;

class QnMediaServerResource;
typedef QnSharedResourcePointer<QnMediaServerResource> QnMediaServerResourcePtr;
typedef QnSharedResourcePointerList<QnMediaServerResource> QnMediaServerResourceList;

class QnFakeMediaServerResource;
typedef QnSharedResourcePointer<QnFakeMediaServerResource> QnFakeMediaServerResourcePtr;

class QnMediaServerUserAttributes;
typedef QSharedPointer<QnMediaServerUserAttributes> QnMediaServerUserAttributesPtr;
typedef QList<QnMediaServerUserAttributesPtr> QnMediaServerUserAttributesList;

class QnPlColdStoreStorage;
typedef QnSharedResourcePointer<QnPlColdStoreStorage> QnPlColdStoreStoragePtr;
typedef QnSharedResourcePointerList<QnPlColdStoreStorage> QnPlColdStoreStorageList;

class QnPlAxisResource;
typedef QnSharedResourcePointer<QnPlAxisResource> QnPlAxisResourcePtr;
typedef QnSharedResourcePointerList<QnPlAxisResource> QnPlAxisResourceList;

class QnActiResource;
typedef QnSharedResourcePointer<QnActiResource> QnActiResourcePtr;
typedef QnSharedResourcePointerList<QnActiResource> QnActiResourceList;

class QnStardotResource;
typedef QnSharedResourcePointer<QnStardotResource> QnStardotResourcePtr;
typedef QnSharedResourcePointerList<QnStardotResource> QnStardotResourceList;

class QnPlOnvifResource;
typedef QnSharedResourcePointer<QnPlOnvifResource> QnPlOnvifResourcePtr;
typedef QnSharedResourcePointerList<QnPlOnvifResource> QnPlOnvifResourceList;

class QnArchiveCamResource;
typedef QnSharedResourcePointer<QnArchiveCamResource> QnArchiveCamResourcePtr;
typedef QnSharedResourcePointerList<QnArchiveCamResource> QnArchiveCamResourceList;

class QnPlIsdResource;
typedef QnSharedResourcePointer<QnPlIsdResource> QnPlIsdResourcePtr;
typedef QnSharedResourcePointerList<QnPlIsdResource> QnPlIsdResourceList;

class QnPlPulseResource;
typedef QnSharedResourcePointer<QnPlPulseResource> QnPlPulseResourcePtr;
typedef QnSharedResourcePointerList<QnPlPulseResource> QnPlPulseResourceList;

class QnPlIqResource;
typedef QnSharedResourcePointer<QnPlIqResource> QnPlIqResourcePtr;
typedef QnSharedResourcePointerList<QnPlIqResource> QnPlIqResourceList;

class QnDigitalWatchdogResource;
typedef QnSharedResourcePointer<QnDigitalWatchdogResource> QnDigitalWatchdogResourcePtr;
typedef QnSharedResourcePointerList<QnDigitalWatchdogResource> QnDigitalWatchdogResourceList;

class QnVistaResource;
typedef QnSharedResourcePointer<QnVistaResource> QnVistaResourcePtr;

class QnDesktopResource;
typedef QnSharedResourcePointer<QnDesktopResource> QnDesktopResourcePtr;

class QnThirdPartyResource;
typedef QnSharedResourcePointer<QnThirdPartyResource> QnThirdPartyResourcePtr;

class QnVideoWallResource;
typedef QnSharedResourcePointer<QnVideoWallResource> QnVideoWallResourcePtr;
typedef QnSharedResourcePointerList<QnVideoWallResource> QnVideoWallResourceList;

class QnVideoWallItemIndex;
typedef QList<QnVideoWallItemIndex> QnVideoWallItemIndexList;

class QnVideoWallMatrixIndex;
typedef QList<QnVideoWallMatrixIndex> QnVideoWallMatrixIndexList;

class QnAviResource;
typedef QnSharedResourcePointer<QnAviResource> QnAviResourcePtr;

class QnWebPageResource;
typedef QnSharedResourcePointer<QnWebPageResource> QnWebPageResourcePtr;
typedef QnSharedResourcePointerList<QnWebPageResource> QnWebPageResourceList;

class QnFlirEIPResource;
typedef QnSharedResourcePointer<QnFlirEIPResource> QnFlirEIPResourcePtr;
typedef QnSharedResourcePointerList<QnFlirEIPResource> QnFlirEIPResourceList;

namespace nx {
namespace plugins {
namespace flir {

class FcResource;
class OnvifResource;

} // namespace flir
} // namespace plugins
} // namespace nx

typedef QnSharedResourcePointer<nx::plugins::flir::FcResource> QnFlirFcResourcePtr;
typedef QnSharedResourcePointerList<nx::plugins::flir::FcResource> QnFlirFcResourceList;

typedef QnSharedResourcePointer<nx::plugins::flir::OnvifResource> QnFlirOnvifResourcePtr;
typedef QnSharedResourcePointerList<nx::plugins::flir::OnvifResource> QnFlirOnvifResourceList;

class QnAdamResource;
typedef QnSharedResourcePointer<QnAdamResource> QnAdamResourcePtr;

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HikvisionResource;
class LilinResource;
class HanwhaResource;

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

typedef QnSharedResourcePointer<nx::mediaserver_core::plugins::LilinResource> LilinResourcePtr;

typedef
QnSharedResourcePointer<nx::mediaserver_core::plugins::HikvisionResource> QnHikvisionResourcePtr;

typedef
QnSharedResourcePointer<nx::mediaserver_core::plugins::HanwhaResource> HanwhaResourcePtr;
