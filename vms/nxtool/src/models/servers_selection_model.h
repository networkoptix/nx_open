
#pragma once

#include <QtQml>
#include <QAbstractListModel>

#include <base/types.h>
#include <nx/vms/server/api/client.h>

namespace rtu
{
    class ServersSelectionModel : public QAbstractListModel
    {
        Q_OBJECT

        Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
        Q_PROPERTY(int serversCount READ serversCount NOTIFY serversCountChanged)

    public:
        ServersSelectionModel(QObject *parent = nullptr);

        virtual ~ServersSelectionModel();

        typedef nx::vms::server::api::ServerInfoPtrContainer ServerInfoPtrContainer;
        typedef nx::vms::server::api::ServerInfo ServerInfo;
        typedef nx::vms::server::api::BaseServerInfo BaseServerInfo;
        typedef nx::vms::server::api::ExtraServerInfo ExtraServerInfo;
        typedef nx::vms::server::api::InterfaceInfoList InterfaceInfoList;
        typedef nx::vms::server::api::HttpAccessMethod HttpAccessMethod;

    public slots:
        /// Sets items with specified ids selected
        void setSelectedItems(const IDsVector &ids);

        void changeItemSelectedState(int rowIndex);

        void setItemSelected(int rowIndex);

        void setAllItemSelected(bool selected);

        void tryLoginWith(const QString &primarySystem
            , const QString &password
            , const Callback &callback);

        void blinkForItem(int row);

        ///

        int selectedCount() const;

        ServerInfoPtrContainer selectedServers();

        int serversCount() const;

        ///

        void serverDiscovered(const BaseServerInfo &baseInfo);

        void addServer(const BaseServerInfo &baseInfo);

        void changeServer(const BaseServerInfo &baseInfo);

        void removeServers(const IDsVector &removed);

        void unknownAdded(const QString &address);

        void unknownRemoved(const QString &address);

        void updateExtraInfo(const QUuid &id
            , const ExtraServerInfo &extraInfo
            , const QString &hostName);

        void updateTimeDateInfo(const QUuid &id
            , qint64 utcDateTimeMs
            , const QByteArray &timeZoneId
            , qint64 timestampMs);

        void updateInterfacesInfo(const QUuid &id
            , const QString &host
            , const InterfaceInfoList &interfaces);

        void updateSystemNameInfo(const QUuid &id
            , const QString &systemName);

        void updatePortInfo(const QUuid &id
            , int port);

        void updatePasswordInfo(const QUuid &id
            , const QString &password);

        void lockItems(const IDsVector &ids
            , const QString &reason);

        void unlockItems(const IDsVector &ids);

        void changeAccessMethod(const QUuid& id, HttpAccessMethod newHttpAccessMethod);

    signals:
        void blinkAtSystem(int systemItemIndex);

        void layoutChanged();

        void selectionChanged();    /// Signals that new items are selected or
                                    /// selection has got empty

        void updateSelectionData();

        void serversCountChanged();

        void serverLogged(const ServerInfo &info);

    private:
        int rowCount(const QModelIndex &parent = QModelIndex()) const;

        QVariant data(const QModelIndex &index
            , int role = Qt::DisplayRole) const;

        Roles roleNames() const;

    private:
        class Impl;
        Impl * const m_impl;
    };
}

