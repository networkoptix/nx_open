// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

#include <client/client_globals.h>
#include <core/resource/layout_item_data.h>
#include <nx/utils/uuid.h>
#include <recording/time_period.h>

namespace nx::vms::client::desktop {

/**
 * Serialized workbench state. Does not contain the actual layout data, so suitable for restoring
 * the state once layouts are loaded.
 */
struct WorkbenchState
{
public:
    struct UnsavedLayout
    {
        nx::Uuid id;
        nx::Uuid parentId;
        QString name;
        qreal cellSpacing = 0.0;
        float cellAspectRatio = 0.0;
        QString backgroundImageFilename;
        qreal backgroundOpacity = 0.0;
        QSize backgroundSize;
        nx::vms::common::LayoutItemDataList items;
        bool isCrossSystem = false;
    };

    nx::Uuid userId; //< Id of a User. Works as the first part of the key.
    nx::Uuid localSystemId; //< Id of a System. Works as the second part of the key.
    nx::Uuid currentLayoutId;
    nx::Uuid runningTourId;
    QList<nx::Uuid> layoutUuids;
    QList<UnsavedLayout> unsavedLayouts;
};

#define WorkbenchStateUnsavedLayoutItem_Fields (resource)(uuid)(flags)(combinedGeometry) \
    (zoomTargetUuid)(zoomRect)(rotation)(displayInfo)(controlPtz)(displayAnalyticsObjects) \
    (displayRoi)(contrastParams)(dewarpingParams)(displayHotspots)
#define WorkbenchStateUnsavedLayout_Fields (id)(parentId)(name)(cellSpacing)(cellAspectRatio) \
    (backgroundImageFilename)(backgroundOpacity)(backgroundSize)(items)(isCrossSystem)
#define WorkbenchState_Fields \
    (userId)(localSystemId)(currentLayoutId)(runningTourId)(layoutUuids)

QN_FUSION_DECLARE_FUNCTIONS(WorkbenchState, (json))
QN_FUSION_DECLARE_FUNCTIONS(WorkbenchState::UnsavedLayout, (json))

} // namespace nx::vms::client::desktop
