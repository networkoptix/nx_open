// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_item_model.h"

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {
namespace test {

QModelIndex TestItemModel::buildIndex(std::initializer_list<int> indices) const
{
    QModelIndex current;
    for (int row: indices)
    {
        NX_ASSERT(row >= 0 && row < rowCount(current));
        current = index(row, 0, current);
    }

    return current;
}

} // namespace test
} // namespace nx::vms::client::desktop
