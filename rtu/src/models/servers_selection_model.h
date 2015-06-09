
#pragma once

#include <QtQml>
#include <QAbstractListModel>

#include <server_info.h>
#include <models/types.h>

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

        void selectionPasswordChanged(const QString &password);
        
        void tryLoginWith(const QString &password);

        ///
        
        void updateTimeDateInfo(const QUuid &id
            , const QDateTime &dateTime
            , const QDateTime &timestamp);
        
        void updateInterfacesInfo(const QUuid &id
            , const InterfaceInfoList &interfaces);

        void updateSystemNameInfo(const QUuid &id
            , const QString &systemName);
        
        void updatePortInfo(const QUuid &id
            , int port);
        
        ///

        int selectedCount() const;
        
        ServerInfoList selectedServers();
        
    signals:
        void selectionChanged();
        
        void serverInfoChanged(ServerInfo *info);
        
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

