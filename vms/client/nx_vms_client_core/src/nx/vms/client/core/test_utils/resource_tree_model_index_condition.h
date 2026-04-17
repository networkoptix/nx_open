// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QModelIndex>

#include <nx/vms/client/core/resource_views/data/resource_extra_status.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>

namespace nx::vms::client::core {
namespace test {
namespace index_condition {

using Condition = std::function<bool(const QModelIndex& index)>;

// Ancestry and arrangement conditions.
NX_VMS_CLIENT_CORE_API Condition topLevelNode();
NX_VMS_CLIENT_CORE_API Condition hasChildren();
NX_VMS_CLIENT_CORE_API Condition hasNoChildren();
NX_VMS_CLIENT_CORE_API Condition hasExactChildrenCount(int count);
NX_VMS_CLIENT_CORE_API Condition directChildOf(Condition paramCondition);
NX_VMS_CLIENT_CORE_API Condition directChildOf(const QModelIndex& index);
NX_VMS_CLIENT_CORE_API Condition atRow(int row);

// Display text conditions.
NX_VMS_CLIENT_CORE_API Condition displayFullMatch(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
NX_VMS_CLIENT_CORE_API Condition displayContains(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
NX_VMS_CLIENT_CORE_API Condition displayStartsWith(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
NX_VMS_CLIENT_CORE_API Condition displayEndsWith(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
NX_VMS_CLIENT_CORE_API Condition displayEmpty();
NX_VMS_CLIENT_CORE_API Condition extraInfoEmpty();

// Icon conditions.
NX_VMS_CLIENT_CORE_API Condition iconFullMatch(core::ResourceIconCache::Key paramIconKey);
NX_VMS_CLIENT_CORE_API Condition iconTypeMatch(core::ResourceIconCache::Key paramIconKey);
NX_VMS_CLIENT_CORE_API Condition iconStatusMatch(core::ResourceIconCache::Key paramIconKey);

// Flags conditions.
NX_VMS_CLIENT_CORE_API Condition hasFlag(Qt::ItemFlag flag);
NX_VMS_CLIENT_CORE_API Condition flagsMatch(Qt::ItemFlags flags);

// Extra status flags conditions.
NX_VMS_CLIENT_CORE_API Condition hasResourceExtraStatusFlag(core::ResourceExtraStatusFlag flag);
NX_VMS_CLIENT_CORE_API Condition resourceExtraStatusFlagsMatch(core::ResourceExtraStatus flags);

// Generic data conditions.
NX_VMS_CLIENT_CORE_API Condition dataMatch(int role, const QVariant& data);
NX_VMS_CLIENT_CORE_API Condition nodeTypeDataMatch(core::ResourceTree::NodeType nodeType);

// Resource nodes lookup conditions.
NX_VMS_CLIENT_CORE_API Condition serverIconTypeCondition();
NX_VMS_CLIENT_CORE_API Condition cameraIconTypeCondition();
NX_VMS_CLIENT_CORE_API Condition reducedEdgeCameraCondition();

// Top level nodes lookup conditions.
NX_VMS_CLIENT_CORE_API Condition localFilesNodeCondition();
NX_VMS_CLIENT_CORE_API Condition videoWallNodeCondition();
NX_VMS_CLIENT_CORE_API Condition showreelsNodeCondition();
NX_VMS_CLIENT_CORE_API Condition layoutsNodeCondition();
NX_VMS_CLIENT_CORE_API Condition integrationsNodeCondition();
NX_VMS_CLIENT_CORE_API Condition webPagesNodeCondition();
NX_VMS_CLIENT_CORE_API Condition serversNodeCondition();

// Logical conditions.
NX_VMS_CLIENT_CORE_API Condition non(Condition paramCondition);

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
} // namespace nx::vms::client::core
