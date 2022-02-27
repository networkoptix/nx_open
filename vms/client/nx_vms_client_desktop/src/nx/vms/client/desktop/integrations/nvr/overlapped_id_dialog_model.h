// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QAbstractItemModel>

namespace nx::vms::client::desktop::integrations {

class OverlappedIdDialogModel: public QAbstractItemModel
{
    Q_OBJECT

    using base_type = QAbstractItemModel;

public:
    enum Roles
    {
        IdRole = Qt::UserRole
    };

public:
    explicit OverlappedIdDialogModel(QObject* parent = nullptr);

    void setIdList(const std::vector<int>& idList);

public:
    virtual QHash<int, QByteArray> roleNames() const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual QModelIndex index(
        int row,
        int column,
        const QModelIndex& parent = QModelIndex()) const override;

    virtual QModelIndex parent(const QModelIndex& index) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

private:
    std::vector<int> m_idList;
};

} // namespace nx::vms::client::desktop::integrations
