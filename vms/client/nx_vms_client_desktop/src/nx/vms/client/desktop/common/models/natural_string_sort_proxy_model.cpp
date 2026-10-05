// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "natural_string_sort_proxy_model.h"

#include <nx/utils/string.h>

namespace nx::vms::client::desktop {

NaturalStringSortProxyModel::NaturalStringSortProxyModel(QObject* parent):
    base_type(parent),
    m_collator(nx::utils::createCollator(sortCaseSensitivity()))
{
    setCustomLessThan(
        [this](const QModelIndex& left, const QModelIndex& right) -> bool
        {
            if (m_collator.caseSensitivity() != sortCaseSensitivity())
                m_collator.setCaseSensitivity(sortCaseSensitivity());

            const QString leftName = left.data(Qt::DisplayRole).toString();
            const QString rightName = right.data(Qt::DisplayRole).toString();
            return m_collator.compare(leftName, rightName) < 0;
        });
}

} // namespace nx::vms::client::desktop
