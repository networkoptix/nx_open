// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <QtCore/QAbstractProxyModel>

#include <api/server_rest_connection.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop::integrations {

class ImportFromDeviceDialogModel: public QAbstractProxyModel
{
    Q_OBJECT

    using base_type = QAbstractProxyModel;

public:
    explicit ImportFromDeviceDialogModel(QObject* parent = nullptr);
    virtual ~ImportFromDeviceDialogModel() override;

    void setData(const nx::vms::api::RemoteArchiveSynchronizationStatusList& data);

public:
    virtual QHash<int, QByteArray> roleNames() const override;

    virtual QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    virtual QModelIndex index(
        int row,
        int column,
        const QModelIndex& parent = QModelIndex()) const override;

    virtual QModelIndex parent(const QModelIndex& index) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::integrations
