// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QModelIndex>

#include <nx/vms/client/desktop/resource_views/data/resource_extra_status.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>

namespace nx::vms::client::desktop {
namespace test {
namespace index_condition {

using Condition = std::function<bool(const QModelIndex& index)>;

// Ancestry and arrangement conditions.
Condition topLevelNode();
Condition hasChildren();
Condition hasNoChildren();
Condition hasExactChildrenCount(int count);
Condition directChildOf(Condition paramCondition);
Condition directChildOf(const QModelIndex& index);
Condition atRow(int row);

// Display text conditions.
Condition displayFullMatch(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
Condition displayContains(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
Condition displayStartsWith(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
Condition displayEndsWith(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
Condition displayEmpty();
Condition extraInfoEmpty();

// Icon conditions.
Condition iconFullMatch(QnResourceIconCache::Key paramIconKey);
Condition iconTypeMatch(QnResourceIconCache::Key paramIconKey);
Condition iconStatusMatch(QnResourceIconCache::Key paramIconKey);

// Flags conditions.
Condition hasFlag(Qt::ItemFlag flag);
Condition flagsMatch(Qt::ItemFlags flags);

// Extra status flags conditions.
Condition hasResourceExtraStatusFlag(ResourceExtraStatusFlag flag);
Condition resourceExtraStatusFlagsMatch(ResourceExtraStatus flags);

// Generic data conditions.
Condition dataMatch(int role, const QVariant& data);
Condition nodeTypeDataMatch(ResourceTree::NodeType nodeType);
Condition mimeDataContainsResource(const QnResourcePtr& resource);
Condition mimeDataContainsEntity(const nx::Uuid& entityId);

// Resource nodes lookup conditions.
Condition serverIconTypeCondition();
Condition cameraIconTypeCondition();
Condition reducedEdgeCameraCondition();

// Top level nodes lookup conditions.
Condition localFilesNodeCondition();
Condition videoWallNodeCondition();
Condition showreelsNodeCondition();
Condition layoutsNodeCondition();
Condition integrationsNodeCondition();
Condition webPagesNodeCondition();
Condition serversNodeCondition();

// Shared resources and layouts nodes lookup conditions.
Condition sharedResourcesNodeCondition();
Condition sharedLayoutsNodeCondition();
Condition mixedLayoutsNodeCondition(); //< "Layouts" text, plain icon, but sharedLayouts node type.

// Shared resources and layouts placeholder nodes lookup conditions.
Condition allCamerasAndResourcesNodeCondition();
Condition allSharedLayoutsNodeCondition();

// Logical conditions.
Condition non(Condition paramCondition);

template <class T>
T allOf(T condition)
{
    return condition;
}

template <class T, class... Args>
T allOf(T condition, Args... conditions)
{
    return
        [condition, nextConditions = allOf(conditions...)](const QModelIndex& index)
        {
            return condition(index) && nextConditions(index);
        };
}

template <class T>
T anyOf(T condition)
{
    return condition;
}

template <class T, class... Args>
T anyOf(T condition, Args... conditions)
{
    return
        [condition, nextConditions = anyOf(conditions...)](const QModelIndex& index)
        {
            return condition(index) || nextConditions(index);
        };
}

} // namespace index_condition
} // namespace test
} // namespace nx::vms::client::desktop
