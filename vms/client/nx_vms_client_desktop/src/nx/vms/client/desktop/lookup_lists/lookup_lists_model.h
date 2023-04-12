// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/vms/api/data/lookup_list_data.h>

namespace nx::vms::client::desktop {

/** List of Lookup Lists. */
class LookupListsModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

    enum DataRole
    {
        ValueRole = Qt::UserRole
    };

public:
    explicit LookupListsModel(QObject* parent = nullptr);
    virtual ~LookupListsModel();

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void resetData(const nx::vms::api::LookupListDataList& data);
    void addList(const nx::vms::api::LookupListData& list);
    void updateList(const nx::vms::api::LookupListData& list);
    void removeList(const QnUuid& id);

private:
    std::vector<nx::vms::api::LookupListData> m_data;
};

} // namespace nx::vms::client::desktop
