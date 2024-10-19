// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QFlags>

#include <nx/vms/client/core/enums.h>
#include <ui/models/sort_filter_list_model.h>
#include <ui/models/systems_model.h>

class QnOrderedSystemsModel: public QnSortFilterListModel
{
    Q_OBJECT
    using base_type = QnSortFilterListModel;

public:
    QnOrderedSystemsModel(QObject* parent = nullptr);

    static Q_INVOKABLE int searchRoleId();

private:
    bool lessThan(
        const QModelIndex& left,
        const QModelIndex& right) const override;
};
