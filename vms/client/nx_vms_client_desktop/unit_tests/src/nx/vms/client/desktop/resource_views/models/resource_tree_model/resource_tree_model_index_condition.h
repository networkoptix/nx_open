#pragma once

#include <vector>

#include <QtCore/QModelIndex>
#include <ui/style/resource_icon_cache.h>

namespace nx::vms::client::desktop {
namespace test {
namespace index_condition {

using Condition = std::function<bool(const QModelIndex& index)>;

// Ancestry and arrangement conditions.
Condition topLevelNode();
Condition hasChildren();
Condition directChildOf(Condition paramCondition);
Condition atRow(int row);

// Display text conditions.
Condition displayFullMatch(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
Condition displayContains(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
Condition displayStartsWith(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
Condition displayEndsWith(const QString& string, Qt::CaseSensitivity cs = Qt::CaseSensitive);
Condition displayEmpty();

// Icon conditions.
Condition iconFullMatch(QnResourceIconCache::Key paramIconKey);
Condition iconTypeMatch(QnResourceIconCache::Key paramIconKey);
Condition iconStatusMatch(QnResourceIconCache::Key paramIconKey);

// Flags conditions.
Condition hasFlag(Qt::ItemFlag flag);
Condition flagsMatch(Qt::ItemFlags flags);

// Generic data conditions.
Condition dataMatch(int role, const QVariant& data);

// Top level node lookup conditions.
Condition localFilesNodeCondition();
Condition videoWallNodeCondition();
Condition showreelsNodeCondition();
Condition layoutsNodeCondition();
Condition webPagesNodeCondition();

// Logical conditions.
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

template <class T>
T noneOf(T condition)
{
    return
        [condition](const QModelIndex& index)
        {
            return !condition(index);
        };
}

template <class T, class... Args>
T noneOf(T condition, Args... conditions)
{
    return
        [condition, nextConditions = noneOf(conditions...)](const QModelIndex& index)
        {
            return !condition(index) && nextConditions(index);
        };
}

} // namespace index_condition
} // namespace test
} // namespace nx::vms::client::desktop
