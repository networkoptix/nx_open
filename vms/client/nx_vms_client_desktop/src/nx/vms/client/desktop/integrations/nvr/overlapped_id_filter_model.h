// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/models/sort_filter_list_model.h>

namespace nx::vms::client::desktop::integrations {

class OverlappedIdFilterModel: public QnSortFilterListModel
{
    Q_OBJECT

public:
    explicit OverlappedIdFilterModel(QObject* parent = nullptr);

private:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

} // namespace nx::vms::client::desktop::integrations
