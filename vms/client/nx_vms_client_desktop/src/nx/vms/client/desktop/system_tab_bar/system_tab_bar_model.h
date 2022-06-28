// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <finders/abstract_systems_finder.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>

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
    };

    SystemTabBarModel(QObject* parent = nullptr);
    virtual ~SystemTabBarModel();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addSystem(const QnSystemDescriptionPtr& systemDescription, const LogonData& logonData);
    void removeSystem(const QnSystemDescriptionPtr& systemDescription);
    void removeSystem(const QString& systemId);

private:
    QList<SystemData> m_systems;
};

} // namespace nx::vms::client::desktop
