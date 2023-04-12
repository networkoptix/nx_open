// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractTableModel>

#include <nx/vms/api/data/lookup_list_data.h>

namespace nx::vms::client::desktop {

/** Entries of a single Lookup List. */
class LookupListModel: public QAbstractTableModel
{
    Q_OBJECT
    using base_type = QAbstractTableModel;

    enum DataRole
    {
        TypeRole = Qt::UserRole
    };

public:
    explicit LookupListModel(QObject* parent = nullptr);
    virtual ~LookupListModel();

    virtual QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    void resetData(nx::vms::api::LookupListData data);
    void addEntry(nx::vms::api::LookupListEntry entry);

private:
    nx::vms::api::LookupListData m_data;
};

} // namespace nx::vms::client::desktop
