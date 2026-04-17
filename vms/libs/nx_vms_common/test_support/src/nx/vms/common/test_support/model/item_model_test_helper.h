// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QAbstractItemModel>

namespace nx::vms::common::test {

class NX_VMS_COMMON_TEST_SUPPORT_API ItemModelTestHelper
{
public:
    virtual ~ItemModelTestHelper() = default;

    using Condition = std::function<bool(const QModelIndex& index)>;

    virtual QAbstractItemModel* model() const = 0;

    QModelIndexList getAllIndexes() const;

    QModelIndex firstMatchingIndex(Condition condition) const;
    QModelIndex uniqueMatchingIndex(Condition condition) const;
    QModelIndexList allMatchingIndexes(Condition condition) const;

    bool noneMatches(Condition condition) const;
    bool onlyOneMatches(Condition condition) const;
    bool atLeastOneMatches(Condition condition) const;
    int matchCount(Condition condition) const;

    std::vector<QString> transformToDisplayStrings(const QModelIndexList& indexes) const;
};

} // namespace nx::vms::common::test
