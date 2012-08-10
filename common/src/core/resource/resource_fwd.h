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
typedef QnSharedResourcePointerList<QnResourcePtr> QnResourceList;

class QnVirtualCameraResource;
typedef QnSharedResourcePointer<QnVirtualCameraResource> QnVirtualCameraResourcePtr;
typedef QnSharedResourcePointerList<QnVirtualCameraResourcePtr> QnVirtualCameraResourceList;

class QnPhysicalCameraResource;
typedef QnSharedResourcePointer<QnPhysicalCameraResource> QnPhysicalCameraResourcePtr;
typedef QnSharedResourcePointerList<QnPhysicalCameraResourcePtr> QnPhysicalCameraResourceList;

class QnLayoutResource;
typedef QnSharedResourcePointer<QnLayoutResource> QnLayoutResourcePtr;
typedef QnSharedResourcePointerList<QnLayoutResourcePtr> QnLayoutResourceList;

class QnLayoutItemIndex;
typedef QnSharedResourcePointerList<QnLayoutItemIndex> QnLayoutItemIndexList;

class QnMediaResource;
typedef QnSharedResourcePointer<QnMediaResource> QnMediaResourcePtr;
typedef QnSharedResourcePointerList<QnMediaResourcePtr> QnMediaResourceList;

class QnAbstractArchiveResource;
typedef QnSharedResourcePointer<QnAbstractArchiveResource> QnAbstractArchiveResourcePtr;
typedef QnSharedResourcePointerList<QnAbstractArchiveResource> QnAbstractArchiveResourceList;

class QnNetworkResource;
typedef QnSharedResourcePointer<QnNetworkResource> QnNetworkResourcePtr;
typedef QnSharedResourcePointerList<QnNetworkResourcePtr> QnNetworkResourceList;

class QnSecurityCamResource;
typedef QnSharedResourcePointer<QnSecurityCamResource> QnSecurityCamResourcePtr;
typedef QnSharedResourcePointerList<QnSecurityCamResourcePtr> QnSecurityCamResourceList;

class QnStorageResource;
typedef QnSharedResourcePointer<QnStorageResource> QnStorageResourcePtr;

class QnAbstractStorageResource;
typedef QnSharedResourcePointer<QnAbstractStorageResource> QnAbstractStorageResourcePtr;
typedef QnSharedResourcePointerList<QnAbstractStorageResourcePtr> QnAbstractStorageResourceList;

class QnUserResource;
typedef QnSharedResourcePointer<QnUserResource> QnUserResourcePtr;
typedef QnSharedResourcePointerList<QnUserResourcePtr> QnUserResourceList;

class QnVideoServerResource;
typedef QnSharedResourcePointer<QnVideoServerResource> QnVideoServerResourcePtr;
typedef QnSharedResourcePointerList<QnVideoServerResourcePtr> QnVideoServerResourceList;

class QnLocalVideoServerResource;
typedef QnSharedResourcePointer<QnLocalVideoServerResource> QnLocalVideoServerResourcePtr;
typedef QnSharedResourcePointerList<QnLocalVideoServerResourcePtr> QnLocalVideoServerResourceList;

class QnPlColdStoreStorage;
typedef QnSharedResourcePointer<QnPlColdStoreStorage> QnPlColdStoreStoragePtr;

class QnPlAxisResource;
typedef QnSharedResourcePointer<QnPlAxisResource> QnPlAxisResourcePtr;

class QnPlOnvifResource;
typedef QnSharedResourcePointer<QnPlOnvifResource> QnPlOnvifResourcePtr;

class QnPlIsdResource;
typedef QnSharedResourcePointer<QnPlIsdResource> QnPlIsdResourcePtr;

#endif // QN_RESOURCE_FWD_H
