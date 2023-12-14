// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>

#include <nx/vms/client/core/common/models/standard_item_model.h>

namespace nx::vms::client::desktop {
namespace test {

class TestItemModel: public nx::vms::client::core::StandardItemModel
{
public:
    QModelIndex buildIndex(std::initializer_list<int> indices) const;
};

} // namespace test
} // namespace nx::vms::client::desktop

// -----------------------------------------------------------------------------------------------
// Some utility functions.

inline QString toString(const QModelIndex& index)
{
    if (!index.isValid())
        return "empty";

    return index.parent().isValid()
        ? QString("%1:%2").arg(toString(index.parent()), QString::number(index.row()))
        : QString::number(index.row());
}

inline std::ostream& operator<<(std::ostream& s, const QModelIndex& index)
{
    return s << toString(index).toStdString();
}

inline std::ostream& operator<<(std::ostream& s, const QPersistentModelIndex& index)
{
    return s << toString(index).toStdString();
}
