// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCollator>
#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/string.h>

namespace nx::vms::client::desktop {

class IoPortsSortModel: public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel; //< Forward constructors.

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QCollator m_collator{nx::utils::createCollator()};
};

} // namespace nx::vms::client::core
