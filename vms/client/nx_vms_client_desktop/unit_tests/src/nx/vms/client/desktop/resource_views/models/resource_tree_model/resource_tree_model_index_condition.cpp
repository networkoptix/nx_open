#include "resource_tree_model_index_condition.h"

#include <client/client_globals.h>

namespace nx::vms::client::desktop {
namespace test {
namespace index_condition {

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

Condition directChildOf(Condition paramCondition)
{
    return
        [paramCondition](const QModelIndex& index)
        {
            return index.isValid() && paramCondition(index.parent());
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

Condition iconFullMatch(QnResourceIconCache::Key paramIconKey)
{
    return
        [paramIconKey](const QModelIndex& index)
        {
            if (!index.isValid())
                return false;
            const auto iconKey =
                static_cast<QnResourceIconCache::Key>(
                    index.data(Qn::ResourceIconKeyRole).toInt());
            return iconKey == paramIconKey;
        };
}

Condition iconTypeMatch(QnResourceIconCache::Key paramIconKey)
{
    return
        [paramIconKey](const QModelIndex& index)
        {
            if (!index.isValid())
                return false;
            const auto iconKey =
                static_cast<QnResourceIconCache::Key>(
                    index.data(Qn::ResourceIconKeyRole).toInt());

            const auto iconType = iconKey & QnResourceIconCache::TypeMask;
            const auto paramIconType = paramIconKey & QnResourceIconCache::TypeMask;

            return iconType == paramIconType;
        };
}

Condition iconStatusMatch(QnResourceIconCache::Key paramIconKey)
{
    return
        [paramIconKey](const QModelIndex& index)
        {
            if (!index.isValid())
                return false;
            const auto iconKey =
                static_cast<QnResourceIconCache::Key>(
                    index.data(Qn::ResourceIconKeyRole).toInt());

            const auto iconType = iconKey & QnResourceIconCache::StatusMask;
            const auto paramIconType = paramIconKey & QnResourceIconCache::StatusMask;

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

Condition dataMatch(int role, const QVariant& paramData)
{
    return
        [role, paramData](const QModelIndex& index)
        {
            return index.isValid() && index.data(role) == paramData;
        };
}

Condition localFilesNodeCondition()
{
    return allOf(
        displayFullMatch("Local Files"),
        iconFullMatch(QnResourceIconCache::LocalResources),
        topLevelNode());
}

Condition videoWallNodeCondition()
{
    return allOf(
        iconTypeMatch(QnResourceIconCache::VideoWall),
        topLevelNode());
}

Condition showreelsNodeCondition()
{
    return allOf(
        displayFullMatch("Showreels"),
        iconFullMatch(QnResourceIconCache::LayoutTours),
        topLevelNode());
}

Condition layoutsNodeCondition()
{
    return allOf(
        displayFullMatch("Layouts"),
        iconFullMatch(QnResourceIconCache::Layouts),
        topLevelNode());
}

Condition webPagesNodeCondition()
{
    return allOf(
        displayFullMatch("Web Pages"),
        iconFullMatch(QnResourceIconCache::WebPages),
        topLevelNode());
}

} // namespace index_condition
} // namespace test
} // namespace nx::vms::client::desktop
