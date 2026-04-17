// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_model_test_helper.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::common::test {

QModelIndexList ItemModelTestHelper::getAllIndexes() const
{
    std::function<QModelIndexList(const QModelIndex&)> getChildren;
    getChildren =
        [this, &getChildren](const QModelIndex& parent)
        {
            QModelIndexList result;
            for (int row = 0; row < model()->rowCount(parent); ++row)
            {
                const auto childIndex = model()->index(row, 0, parent);
                result.append(childIndex);
                result.append(getChildren(childIndex));
            }
            return result;
        };
    return getChildren(QModelIndex());
}

QModelIndex ItemModelTestHelper::firstMatchingIndex(Condition condition) const
{
    const auto matchingIndexes = allMatchingIndexes(condition);
    if (!NX_ASSERT(matchingIndexes.size() > 0))
        return QModelIndex();
    return matchingIndexes.first();
}

QModelIndex ItemModelTestHelper::uniqueMatchingIndex(Condition condition) const
{
    const auto matchingIndexes = allMatchingIndexes(condition);
    if (!NX_ASSERT(matchingIndexes.size() == 1))
        return QModelIndex();
    return matchingIndexes.first();
}

QModelIndexList ItemModelTestHelper::allMatchingIndexes(Condition condition) const
{
    QModelIndexList result;
    const auto allIndexes = getAllIndexes();
    std::ranges::copy_if(allIndexes, std::back_inserter(result), condition);
    return result;
}

bool ItemModelTestHelper::noneMatches(Condition condition) const
{
    return allMatchingIndexes(condition).isEmpty();
}

bool ItemModelTestHelper::onlyOneMatches(Condition condition) const
{
    return allMatchingIndexes(condition).size() == 1;
}

bool ItemModelTestHelper::atLeastOneMatches(Condition condition) const
{
    return allMatchingIndexes(condition).size() > 0;
}

int ItemModelTestHelper::matchCount(Condition condition) const
{
    return allMatchingIndexes(condition).size();
}

std::vector<QString> ItemModelTestHelper::transformToDisplayStrings(
    const QModelIndexList& indexes) const
{
    std::vector<QString> result;
    std::ranges::transform(indexes, std::back_inserter(result),
        [](const QModelIndex& index) { return index.data(Qt::DisplayRole).toString(); });
    return result;
}

} // namespace nx::vms::common::test
