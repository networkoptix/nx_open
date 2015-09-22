
#pragma once

#include <QtQml>
#include <QAbstractListModel>

#include <base/types.h>
#include <base/server_info.h>

namespace rtu
{
    class ServersSelectionModel : public QAbstractListModel
    {
        Q_OBJECT

        Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
        Q_PROPERTY(int serversCount READ serversCount NOTIFY serversCountChanged)
        Q_PROPERTY(bool selectionOutdated READ selectionOutdated NOTIFY selectionOutdatedChanged)

    public:
        ServersSelectionModel(QObject *parent = nullptr);
        
        virtual ~ServersSelectionModel();
        
    public slots:

        /// Sets items with specified ids selected
        void setSelectedItems(const IDsVector &ids);

        void changeItemSelectedState(int rowIndex);
        
        void setItemSelected(int rowIndex);

        void setAllItemSelected(bool selected);
        
        void tryLoginWith(const QString &primarySystem
            , const QString &password
            , const Callback &callback);

        ///

        bool selectionOutdated() const;

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

        void setBusyState(const IDsVector &ids
            , bool isBusy);

        void changeAccessMethod(const QUuid &id
            , bool byHttp);

    signals:
        void layoutChanged();

        void selectionOutdatedChanged();

        void selectionChanged();
        
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

