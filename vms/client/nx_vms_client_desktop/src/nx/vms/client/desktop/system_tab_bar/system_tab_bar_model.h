// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <finders/abstract_systems_finder.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/workbench/state/workbench_state.h>

namespace nx::vms::client::desktop {

class SystemTabBarModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

public:
    struct SystemData
    {
        QnSystemDescriptionPtr systemDescription;
        LogonData logonData;
        WorkbenchState workbenchState;
    };

    SystemTabBarModel(QObject* parent = nullptr);
    virtual ~SystemTabBarModel();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    QModelIndex findSystem(const QnUuid& systemId) const;
    QModelIndex findSystem(const QString& systemId) const;
    void addSystem(const QnSystemDescriptionPtr& systemDescription, const LogonData& logonData);
    void removeSystem(const QnSystemDescriptionPtr& systemDescription);
    void removeSystem(const QString& systemId);
    void setSystemState(const QString& systemId, WorkbenchState state);

private:
    QList<SystemData> m_systems;
};

} // namespace nx::vms::client::desktop
