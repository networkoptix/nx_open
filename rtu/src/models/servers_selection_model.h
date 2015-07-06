
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
    public:
        ServersSelectionModel(QObject *parent = nullptr);
        
        virtual ~ServersSelectionModel();
        
    public slots:
        void changeItemSelectedState(int rowIndex);
        
        void setItemSelected(int rowIndex);

//        void selectionPasswordChanged(const QString &password);
        
        void tryLoginWith(const QString &password);

        ///

        int selectedCount() const;
        
        ServerInfoList selectedServers();

        ///

        void addServer(const BaseServerInfo &baseInfo);

        void changeServer(const BaseServerInfo &baseInfo);

        void removeServers(const IDsVector &removed);

        void updateTimeDateInfo(const QUuid &id
            , const QDateTime &utcDateTime
            , const QTimeZone &timeZone
            , const QDateTime &timestamp);

        void updateInterfacesInfo(const QUuid &id
            , const QString &host
            , const InterfaceInfoList &interfaces);

        void updateSystemNameInfo(const QUuid &id
            , const QString &systemName);

        void updatePortInfo(const QUuid &id
            , int port);

        void updatePasswordInfo(const QUuid &id
            , const QString &password);

    signals:
        void selectionChanged();
        
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

