
#pragma once

#include <QObject>
#include <QDateTime>

#include <base/constants.h>
#include <base/server_info.h>

namespace rtu
{
    class ServersSelectionModel;

    class Selection : public QObject
    {
        Q_OBJECT
        
        Q_PROPERTY(int count READ count NOTIFY changed)
        Q_PROPERTY(int port READ port NOTIFY changed)
        Q_PROPERTY(int flags READ flags NOTIFY changed)
        Q_PROPERTY(int dhcpState READ dhcpState NOTIFY changed)
        Q_PROPERTY(QString systemName READ systemName NOTIFY changed)
        Q_PROPERTY(QString password READ password NOTIFY changed)
        Q_PROPERTY(QDateTime dateTime READ dateTime NOTIFY changed)
        Q_PROPERTY(bool editableInterfaces READ editableInterfaces NOTIFY changed)
        Q_PROPERTY(bool hasEmptyIps READ hasEmptyIps NOTIFY changed)
        
    public:
        explicit Selection(ServersSelectionModel *selectionModel
            , QObject *parent = nullptr);
        
        virtual ~Selection();
        
    public:
        /// Getters for properties
        
        int count() const;
        
        int port() const;
        
        int flags() const;
        
        int dhcpState() const;
        
        const QString &systemName() const;

        const QString &password() const;

        const QDateTime &dateTime() const;
        
        bool editableInterfaces() const;

        bool hasEmptyIps() const;

    public:
        InterfaceInfoList aggregatedInterfaces();

    signals:
        void changed();
        
    private:
        struct Snapshot;
        typedef QScopedPointer<Snapshot> SnapshotPtr;

        SnapshotPtr m_snapshot;
    };
}
