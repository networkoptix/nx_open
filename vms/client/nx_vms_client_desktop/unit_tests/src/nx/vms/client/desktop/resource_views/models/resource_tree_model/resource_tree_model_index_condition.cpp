// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_index_condition.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/utils/mime_data.h>

namespace nx::vms::client::desktop {
namespace test {
namespace index_condition {

using namespace nx::vms::client::core;

Condition topLevelNode()
{
    return
        [](const QModelIndex& index)
        {
            return index.isValid() && index.parent() == QModelIndex();
        };
}

Condition hasChildren()
{
    return
        [](const QModelIndex& index)
        {
            return index.isValid() && index.model()->rowCount(index) > 0;
        };
}

Condition hasNoChildren()
{
    return
        [](const QModelIndex& index)
        {
            return index.isValid() && index.model()->rowCount(index) == 0;
        };
}

Condition hasExactChildrenCount(int count)
{
    return
        [count](const QModelIndex& index)
        {
            return index.isValid() && index.model()->rowCount(index) == count;
        };
}

Condition directChildOf(Condition paramCondition)
{
    return
        [paramCondition](const QModelIndex& index)
        {
            return index.isValid() && paramCondition(index.parent());
        };
}

Condition directChildOf(const QModelIndex& parentIndex)
{
    return
        [parentIndex](const QModelIndex& index)
        {
            return index.isValid() && index.parent() == parentIndex;
        };
}

Condition atRow(int row)
{
    return
        [row](const QModelIndex& index)
        {
            return index.isValid() && (index.row() >= 0) && (index.row() == row);
        };
}

Condition displayFullMatch(const QString& string, Qt::CaseSensitivity caseSensitivity)
{
    return
        [string, caseSensitivity](const QModelIndex& index)
        {
            return index.isValid()
                && index.data(Qt::DisplayRole).toString().compare(string, caseSensitivity) == 0;
        };
}

Condition displayContains(const QString& string, Qt::CaseSensitivity caseSensitivity)
{
    return
        [string, caseSensitivity](const QModelIndex& index)
        {
            return index.isValid()
                && index.data(Qt::DisplayRole).toString().contains(string, caseSensitivity);
        };
}

Condition displayStartsWith(const QString& string, Qt::CaseSensitivity caseSensitivity)
{
    return
        [string, caseSensitivity](const QModelIndex& index)
        {
            return index.isValid()
                && index.data(Qt::DisplayRole).toString().startsWith(string, caseSensitivity);
        };
}

Condition displayEndsWith(const QString& string, Qt::CaseSensitivity caseSensitivity)
{
    return
        [string, caseSensitivity](const QModelIndex& index)
        {
            return index.isValid()
                && index.data(Qt::DisplayRole).toString().endsWith(string, caseSensitivity);
        };
}

Condition displayEmpty()
{
    return
        [](const QModelIndex& index)
        {
            return index.isValid()
                && index.data(Qt::DisplayRole).toString().isEmpty();
        };
}

Condition extraInfoEmpty()
{
    return
        [](const QModelIndex& index)
        {
            return index.isValid()
                && index.data(core::ExtraInfoRole).toString().isEmpty();
        };
}

Condition iconFullMatch(ResourceIconCache::Key paramIconKey)
{
    return
        [paramIconKey](const QModelIndex& index)
        {
            if (!index.isValid())
                return false;
            const auto iconKey =
                static_cast<ResourceIconCache::Key>(
                    index.data(core::ResourceIconKeyRole).toInt());
            return iconKey == paramIconKey;
        };
}

Condition iconTypeMatch(ResourceIconCache::Key paramIconKey)
{
    return
        [paramIconKey](const QModelIndex& index)
        {
            if (!index.isValid())
                return false;
            const auto iconKey =
                static_cast<ResourceIconCache::Key>(
                    index.data(core::ResourceIconKeyRole).toInt());

            const auto iconType = iconKey & ResourceIconCache::TypeMask;
            const auto paramIconType = paramIconKey & ResourceIconCache::TypeMask;

            return iconType == paramIconType;
        };
}

Condition iconStatusMatch(ResourceIconCache::Key paramIconKey)
{
    return
        [paramIconKey](const QModelIndex& index)
        {
            if (!index.isValid())
                return false;
            const auto iconKey =
                static_cast<ResourceIconCache::Key>(
                    index.data(core::ResourceIconKeyRole).toInt());

            const auto iconType = iconKey & ResourceIconCache::StatusMask;
            const auto paramIconType = paramIconKey & ResourceIconCache::StatusMask;

            return iconType == paramIconType;
        };
}

Condition hasFlag(Qt::ItemFlag flag)
{
    return
        [flag](const QModelIndex& index)
        {
            return index.flags().testFlag(flag);
        };
}

Condition flagsMatch(Qt::ItemFlags flags)
{
    return
        [flags](const QModelIndex& index)
        {
            return (index.flags() & flags) == flags;
        };
}

Condition hasResourceExtraStatusFlag(ResourceExtraStatusFlag flag)
{
    return
        [flag](const QModelIndex& index)
        {
            const auto indexData = index.data(core::ResourceExtraStatusRole);
            if (indexData.isNull())
                return false;
            auto indexExtraStatusFlags = indexData.value<ResourceExtraStatus>();
            return indexExtraStatusFlags.testFlag(flag);
        };

}

Condition resourceExtraStatusFlagsMatch(ResourceExtraStatus flags)
{
    return
        [flags](const QModelIndex& index)
        {
            const auto indexData = index.data(core::ResourceExtraStatusRole);
            if (indexData.isNull())
                return false;
            auto indexExtraStatusFlags = indexData.value<ResourceExtraStatus>();
            return indexExtraStatusFlags == flags;
        };
}

Condition dataMatch(int role, const QVariant& paramData)
{
    return
        [role, paramData](const QModelIndex& index)
        {
            return index.isValid() && index.data(role) == paramData;
        };
}

Condition nodeTypeDataMatch(ResourceTree::NodeType nodeType)
{
    return dataMatch(core::NodeTypeRole, QVariant::fromValue(nodeType));
}

Condition mimeDataContainsResource(const QnResourcePtr& resource)
{
    return
        [resource](const QModelIndex& index)
        {
            if (!index.isValid() || resource.isNull() || !resource->resourcePool())
                return false;

            const auto mimeData = MimeData(index.model()->mimeData({index}));
            return mimeData.resources().contains(resource);
        };
}

Condition mimeDataContainsEntity(const nx::Uuid& entityId)
{
    return
        [entityId](const QModelIndex& index)
        {
            if (!index.isValid())
                return false;

            const auto mimeData = MimeData(index.model()->mimeData({index}));
            return mimeData.entities().contains(entityId);
    };
}

Condition serverIconTypeCondition()
{
    return iconTypeMatch(ResourceIconCache::Server);
}

Condition cameraIconTypeCondition()
{
    return iconTypeMatch(ResourceIconCache::Camera);
}

Condition reducedEdgeCameraCondition()
{
    return allOf(
        cameraIconTypeCondition(),
        anyOf(
            directChildOf(serversNodeCondition()),
            topLevelNode()));
}

Condition localFilesNodeCondition()
{
    return allOf(
        displayFullMatch("Local Files"),
        iconFullMatch(ResourceIconCache::LocalResources),
        topLevelNode());
}

Condition videoWallNodeCondition()
{
    return allOf(
        iconTypeMatch(ResourceIconCache::VideoWall),
        topLevelNode());
}

Condition showreelsNodeCondition()
{
    return allOf(
        displayFullMatch("Showreels"),
        iconFullMatch(ResourceIconCache::Showreels),
        topLevelNode());
}

Condition layoutsNodeCondition()
{
    return allOf(
        displayFullMatch("Layouts"),
        iconFullMatch(ResourceIconCache::Layouts),
        topLevelNode());
}

Condition integrationsNodeCondition()
{
    return allOf(
        displayFullMatch("Integrations"),
        iconFullMatch(ResourceIconCache::Integrations),
        topLevelNode());
}

Condition webPagesNodeCondition()
{
    return allOf(
        displayFullMatch("Web Pages"),
        iconFullMatch(ResourceIconCache::WebPages),
        topLevelNode());
}

Condition serversNodeCondition()
{
    return allOf(
        displayFullMatch("Servers"),
        iconFullMatch(ResourceIconCache::Servers),
        topLevelNode());
}

Condition non(Condition paramCondition)
{
    return [paramCondition](const QModelIndex& index)
        {
            return !paramCondition(index);
        };
}

} // namespace index_condition
} // namespace test
} // namespace nx::vms::client::desktop
