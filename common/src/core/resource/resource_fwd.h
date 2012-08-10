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

class QnPhysicalCameraResource;
typedef QnSharedResourcePointer<QnPhysicalCameraResource> QnPhysicalCameraResourcePtr;
typedef QnSharedResourcePointerList<QnPhysicalCameraResource> QnPhysicalCameraResourceList;

class QnLayoutResource;
typedef QnSharedResourcePointer<QnLayoutResource> QnLayoutResourcePtr;
typedef QnSharedResourcePointerList<QnLayoutResource> QnLayoutResourceList;

class QnLayoutItemIndex;
typedef QList<QnLayoutItemIndex> QnLayoutItemIndexList;

class QnMediaResource;
typedef QnSharedResourcePointer<QnMediaResource> QnMediaResourcePtr;
typedef QnSharedResourcePointerList<QnMediaResource> QnMediaResourceList;

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

class QnVideoServerResource;
typedef QnSharedResourcePointer<QnVideoServerResource> QnVideoServerResourcePtr;
typedef QnSharedResourcePointerList<QnVideoServerResource> QnVideoServerResourceList;

class QnLocalVideoServerResource;
typedef QnSharedResourcePointer<QnLocalVideoServerResource> QnLocalVideoServerResourcePtr;
typedef QnSharedResourcePointerList<QnLocalVideoServerResource> QnLocalVideoServerResourceList;

class QnPlColdStoreStorage;
typedef QnSharedResourcePointer<QnPlColdStoreStorage> QnPlColdStoreStoragePtr;
typedef QnSharedResourcePointerList<QnPlColdStoreStorage> QnPlColdStoreStorageList;

class QnPlAxisResource;
typedef QnSharedResourcePointer<QnPlAxisResource> QnPlAxisResourcePtr;
typedef QnSharedResourcePointerList<QnPlAxisResource> QnPlAxisResourceList;

class QnPlOnvifResource;
typedef QnSharedResourcePointer<QnPlOnvifResource> QnPlOnvifResourcePtr;
typedef QnSharedResourcePointerList<QnPlOnvifResource> QnPlOnvifResourceList;

class QnPlIsdResource;
typedef QnSharedResourcePointer<QnPlIsdResource> QnPlIsdResourcePtr;
typedef QnSharedResourcePointerList<QnPlIsdResource> QnPlIsdResourceList;

#endif // QN_RESOURCE_FWD_H
