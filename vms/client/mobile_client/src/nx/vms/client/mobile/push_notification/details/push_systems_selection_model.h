// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <network/cloud_system_data.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::mobile::details {

class PushSystemsSelectionModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

    Q_PROPERTY(QStringList selectedSystems
        READ selectedSystems
        WRITE setSelectedSystems
        NOTIFY selectedSystemsChanged)

    Q_PROPERTY(bool hasChanges
        READ hasChanges
        NOTIFY hasChangesChanged)

public:
    static void registerQmlType();

    PushSystemsSelectionModel(
        const QnCloudSystemList& systems,
        const QStringList& selectedSystems,
        QObject* parent = nullptr);
    virtual ~PushSystemsSelectionModel() override;

    QStringList selectedSystems() const;
    void setSelectedSystems(const QStringList& value);

    bool hasChanges() const;

    Q_INVOKABLE void resetChangesFlag();
    Q_INVOKABLE void setCheckedState(int row, Qt::CheckState state);

public:
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    using RolesHash = QHash<int, QByteArray>;
    virtual RolesHash roleNames() const override;

signals:
    void selectedSystemsChanged();
    void hasChangesChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile::details
