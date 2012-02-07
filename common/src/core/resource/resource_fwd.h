#ifndef QN_RESOURCE_FWD_H
#define QN_RESOURCE_FWD_H

#include <QSharedPointer>
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
typedef QSharedPointer<QnResource> QnResourcePtr;
typedef QList<QnResourcePtr> QnResourceList;

class QnVirtualCameraResource;
typedef QSharedPointer<QnVirtualCameraResource> QnVirtualCameraResourcePtr;
typedef QList<QnVirtualCameraResourcePtr> QnVirtualCameraResourceList;

class QnPhysicalCameraResource;
typedef QSharedPointer<QnPhysicalCameraResource> QnPhysicalCameraResourcePtr;
typedef QList<QnPhysicalCameraResourcePtr> QnPhysicalCameraResourceList;

class QnLayoutResource;
typedef QSharedPointer<QnLayoutResource> QnLayoutResourcePtr;
typedef QList<QnLayoutResourcePtr> QnLayoutResourceList;

class QnLocalFileResource;
typedef QSharedPointer<QnLocalFileResource> QnLocalFileResourcePtr;
typedef QList<QnLocalFileResourcePtr> QnLocalFileResourceList;

class QnMediaResource;
typedef QSharedPointer<QnMediaResource> QnMediaResourcePtr;
typedef QList<QnMediaResourcePtr> QnMediaResourceList;

class QnNetworkResource;
typedef QSharedPointer<QnNetworkResource> QnNetworkResourcePtr;
typedef QList<QnNetworkResourcePtr> QnNetworkResourceList;

class QnSecurityCamResource;
typedef QSharedPointer<QnSecurityCamResource> QnSecurityCamResourcePtr;
typedef QList<QnSecurityCamResourcePtr> QnSecurityCamResourceList;

class QnStorageResource;
typedef QSharedPointer<QnStorageResource> QnStorageResourcePtr;
typedef QList<QnStorageResourcePtr> QnStorageResourceList;

class QnUserResource;
typedef QSharedPointer<QnUserResource> QnUserResourcePtr;
typedef QList<QnUserResourcePtr> QnUserResourceList;


#endif // QN_RESOURCE_FWD_H
