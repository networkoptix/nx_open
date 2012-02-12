#ifndef QN_RESOURCE_FWD_H
#define QN_RESOURCE_FWD_H

#include "shared_resource_pointer.h"
#include <QList>

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
typedef QList<QnResourcePtr> QnResourceList;

class QnVirtualCameraResource;
typedef QnSharedResourcePointer<QnVirtualCameraResource> QnVirtualCameraResourcePtr;
typedef QList<QnVirtualCameraResourcePtr> QnVirtualCameraResourceList;

class QnPhysicalCameraResource;
typedef QnSharedResourcePointer<QnPhysicalCameraResource> QnPhysicalCameraResourcePtr;
typedef QList<QnPhysicalCameraResourcePtr> QnPhysicalCameraResourceList;

class QnLayoutResource;
typedef QnSharedResourcePointer<QnLayoutResource> QnLayoutResourcePtr;
typedef QList<QnLayoutResourcePtr> QnLayoutResourceList;

class QnLocalFileResource;
typedef QnSharedResourcePointer<QnLocalFileResource> QnLocalFileResourcePtr;
typedef QList<QnLocalFileResourcePtr> QnLocalFileResourceList;

class QnMediaResource;
typedef QnSharedResourcePointer<QnMediaResource> QnMediaResourcePtr;
typedef QList<QnMediaResourcePtr> QnMediaResourceList;

class QnNetworkResource;
typedef QnSharedResourcePointer<QnNetworkResource> QnNetworkResourcePtr;
typedef QList<QnNetworkResourcePtr> QnNetworkResourceList;

class QnSecurityCamResource;
typedef QnSharedResourcePointer<QnSecurityCamResource> QnSecurityCamResourcePtr;
typedef QList<QnSecurityCamResourcePtr> QnSecurityCamResourceList;

class QnStorageResource;
typedef QnSharedResourcePointer<QnStorageResource> QnStorageResourcePtr;
typedef QList<QnStorageResourcePtr> QnStorageResourceList;

class QnUserResource;
typedef QnSharedResourcePointer<QnUserResource> QnUserResourcePtr;
typedef QList<QnUserResourcePtr> QnUserResourceList;


#endif // QN_RESOURCE_FWD_H
