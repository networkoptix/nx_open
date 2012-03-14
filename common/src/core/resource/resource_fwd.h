#ifndef QN_RESOURCE_FWD_H
#define QN_RESOURCE_FWD_H

#include "shared_resource_pointer.h"
#include "resource_list.h"

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
typedef QnList<QnResourcePtr> QnResourceList;

class QnVirtualCameraResource;
typedef QnSharedResourcePointer<QnVirtualCameraResource> QnVirtualCameraResourcePtr;
typedef QnList<QnVirtualCameraResourcePtr> QnVirtualCameraResourceList;

class QnPhysicalCameraResource;
typedef QnSharedResourcePointer<QnPhysicalCameraResource> QnPhysicalCameraResourcePtr;
typedef QnList<QnPhysicalCameraResourcePtr> QnPhysicalCameraResourceList;

class QnLayoutResource;
typedef QnSharedResourcePointer<QnLayoutResource> QnLayoutResourcePtr;
typedef QnList<QnLayoutResourcePtr> QnLayoutResourceList;

class QnLayoutItemIndex;
typedef QnList<QnLayoutItemIndex> QnLayoutItemIndexList;

class QnMediaResource;
typedef QnSharedResourcePointer<QnMediaResource> QnMediaResourcePtr;
typedef QnList<QnMediaResourcePtr> QnMediaResourceList;

class QnAbstractArchiveResource;
typedef QnSharedResourcePointer<QnAbstractArchiveResource> QnAbstractArchiveResourcePtr;
typedef QnList<QnAbstractArchiveResource> QnAbstractArchiveResourceList;

class QnNetworkResource;
typedef QnSharedResourcePointer<QnNetworkResource> QnNetworkResourcePtr;
typedef QnList<QnNetworkResourcePtr> QnNetworkResourceList;

class QnSecurityCamResource;
typedef QnSharedResourcePointer<QnSecurityCamResource> QnSecurityCamResourcePtr;
typedef QnList<QnSecurityCamResourcePtr> QnSecurityCamResourceList;

class QnStorageResource;
typedef QnSharedResourcePointer<QnStorageResource> QnStorageResourcePtr;
typedef QnList<QnStorageResourcePtr> QnStorageResourceList;

class QnUserResource;
typedef QnSharedResourcePointer<QnUserResource> QnUserResourcePtr;
typedef QnList<QnUserResourcePtr> QnUserResourceList;

class QnVideoServerResource;
typedef QnSharedResourcePointer<QnVideoServerResource> QnVideoServerResourcePtr;
typedef QnList<QnVideoServerResourcePtr> QnVideoServerResourceList;

class QnLocalVideoServerResource;
typedef QnSharedResourcePointer<QnLocalVideoServerResource> QnLocalVideoServerResourcePtr;
typedef QnList<QnLocalVideoServerResourcePtr> QnLocalVideoServerResourceList;

#endif // QN_RESOURCE_FWD_H
