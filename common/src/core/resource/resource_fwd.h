#ifndef QN_RESOURCE_FWD_H
#define QN_RESOURCE_FWD_H

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

class QnResource;
typedef QnSharedResourcePointer<QnResource> QnResourcePtr;
typedef QnSharedResourcePointerList<QnResource> QnResourceList;

class QnVirtualCameraResource;
typedef QnSharedResourcePointer<QnVirtualCameraResource> QnVirtualCameraResourcePtr;
typedef QnSharedResourcePointerList<QnVirtualCameraResource> QnVirtualCameraResourceList;
typedef QSharedPointer<QnVirtualCameraResourceList> QnVirtualCameraResourceListPtr;

class QnPhysicalCameraResource;
typedef QnSharedResourcePointer<QnPhysicalCameraResource> QnPhysicalCameraResourcePtr;
typedef QnSharedResourcePointerList<QnPhysicalCameraResource> QnPhysicalCameraResourceList;

class QnLayoutResource;
typedef QnSharedResourcePointer<QnLayoutResource> QnLayoutResourcePtr;
typedef QnSharedResourcePointerList<QnLayoutResource> QnLayoutResourceList;

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
typedef QnSharedResourcePointerList<QnSecurityCamResource> QnSecurityCamResourceList;

class QnStorageResource;
typedef QnSharedResourcePointer<QnStorageResource> QnStorageResourcePtr;
typedef QnSharedResourcePointerList<QnStorageResource> QnStorageResourceList;

class QnAbstractStorageResource;
typedef QnSharedResourcePointer<QnAbstractStorageResource> QnAbstractStorageResourcePtr;
typedef QnSharedResourcePointerList<QnAbstractStorageResource> QnAbstractStorageResourceList;

class QnUserResource;
typedef QnSharedResourcePointer<QnUserResource> QnUserResourcePtr;
typedef QnSharedResourcePointerList<QnUserResource> QnUserResourceList;

class QnMediaServerResource;
typedef QnSharedResourcePointer<QnMediaServerResource> QnMediaServerResourcePtr;
typedef QnSharedResourcePointerList<QnMediaServerResource> QnMediaServerResourceList;

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

class QnPlIsdResource;
typedef QnSharedResourcePointer<QnPlIsdResource> QnPlIsdResourcePtr;
typedef QnSharedResourcePointerList<QnPlIsdResource> QnPlIsdResourceList;

class QnPlPulseResource;
typedef QSharedPointer<QnPlPulseResource> QnPlPulseResourcePtr;
typedef QnSharedResourcePointerList<QnPlPulseResource> QnPlPulseResourceList;

class QnPlIqResource;
typedef QSharedPointer<QnPlIqResource> QnPlIqResourcePtr;
typedef QnSharedResourcePointerList<QnPlIqResource> QnPlIqResourceList;

class QnPlWatchDogResource;
typedef QSharedPointer<QnPlWatchDogResource> QnPlWatchDogResourcePtr;
typedef QnSharedResourcePointerList<QnPlWatchDogResource> QnPlWatchDogResourceList;

class QnDesktopResource;
typedef QSharedPointer<QnDesktopResource> QnDesktopResourcePtr;

class QnThirdPartyResource;
typedef QSharedPointer<QnThirdPartyResource> QnThirdPartyResourcePtr;

#endif // QN_RESOURCE_FWD_H
