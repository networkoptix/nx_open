// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <client/client_globals.h>

namespace nx::vms::client::desktop::joystick {

class LayoutIntermidiateModel: public QSortFilterProxyModel
{
    Q_OBJECT

    using base_type = QSortFilterProxyModel;

public:
    enum Roles
    {
        LogicalIdRole = Qn::ItemDataRoleCount + 1
    };
    Q_ENUM(Roles)

public:
    explicit LayoutIntermidiateModel(QObject* parent = nullptr);
    virtual ~LayoutIntermidiateModel() override;

public:
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
};

} // namespace nx::vms::client::desktop::joystick
