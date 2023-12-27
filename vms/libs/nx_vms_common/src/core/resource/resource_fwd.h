// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
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

// TODO: #sivanov Move out the following structs:
// <--
struct QnCameraHistoryItem;

class QnCameraHistory;
typedef QSharedPointer<QnCameraHistory> QnCameraHistoryPtr;
typedef QList<QnCameraHistoryPtr> QnCameraHistoryList;

class QnVideoWallItem;
class QnVideoWallMatrix;
class QnVideoWallControlMessage;

// -->

class QnResourceFactory;
class QnResourcePool;

class QnResource;
using QnResourcePtr = QnSharedResourcePointer<QnResource>;
using QnResourceList = QnSharedResourcePointerList<QnResource>;

class QnResourceType;
typedef QSharedPointer<QnResourceType> QnResourceTypePtr;
typedef QList<QnResourceTypePtr> QnResourceTypeList;

class QnVirtualCameraResource;
typedef QnSharedResourcePointer<QnVirtualCameraResource> QnVirtualCameraResourcePtr;
typedef QnSharedResourcePointerList<QnVirtualCameraResource> QnVirtualCameraResourceList;
typedef QSet<QnVirtualCameraResourcePtr> QnVirtualCameraResourceSet;

class QnLayoutResource;
typedef QnSharedResourcePointer<QnLayoutResource> QnLayoutResourcePtr;
typedef QnSharedResourcePointerList<QnLayoutResource> QnLayoutResourceList;
typedef QSet<QnLayoutResourcePtr> QnLayoutResourceSet;

class QnMediaResource;
typedef QnSharedResourcePointer<QnMediaResource> QnMediaResourcePtr;

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

class QnUserResource;
typedef QnSharedResourcePointer<QnUserResource> QnUserResourcePtr;
typedef QnSharedResourcePointerList<QnUserResource> QnUserResourceList;
typedef QSet<QnUserResourcePtr> QnUserResourceSet;

class QnMediaServerResource;
typedef QnSharedResourcePointer<QnMediaServerResource> QnMediaServerResourcePtr;
typedef QnSharedResourcePointerList<QnMediaServerResource> QnMediaServerResourceList;

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

namespace nx::vms::common {

struct ResourceDescriptor;

struct LayoutItemData;

class AnalyticsPluginResource;
using AnalyticsPluginResourcePtr = QnSharedResourcePointer<AnalyticsPluginResource>;
using AnalyticsPluginResourceList = QnSharedResourcePointerList<AnalyticsPluginResource>;

class AnalyticsEngineResource;
using AnalyticsEngineResourcePtr = QnSharedResourcePointer<AnalyticsEngineResource>;
using AnalyticsEngineResourceList =
    QnSharedResourcePointerList<AnalyticsEngineResource>;

using ResourceFilter = std::function<bool(const QnResourcePtr& resource)>;

template<class Resource>
using ResourceClassFilter = std::function<bool(const QnSharedResourcePointer<Resource>&)>;

} // namespace nx::vms::common
