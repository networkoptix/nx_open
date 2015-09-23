
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

        Q_PROPERTY(int flags READ flags NOTIFY changed)

        Q_PROPERTY(int port READ port NOTIFY changed)
        Q_PROPERTY(int dhcpState READ dhcpState NOTIFY changed)
        Q_PROPERTY(bool editableInterfaces READ editableInterfaces NOTIFY changed)
        Q_PROPERTY(bool hasEmptyIps READ hasEmptyIps NOTIFY changed)
        Q_PROPERTY(QObject *itfSettingsModel READ itfSettingsModel NOTIFY changed)

        Q_PROPERTY(QString systemName READ systemName NOTIFY changed)
        Q_PROPERTY(QString password READ password NOTIFY changed)
        
        Q_PROPERTY(QDateTime dateTime READ dateTime NOTIFY changed)
        Q_PROPERTY(QObject *timeZonesModel READ timeZonesModel NOTIFY changed)

    public:
        explicit Selection(ServersSelectionModel *selectionModel
            , QObject *parent = nullptr);
        
        virtual ~Selection();

        void updateSelection(ServersSelectionModel *selectionModel);

    public:
        /// Getters for properties
        
        int count() const;
        
        int flags() const;

        ///

        int port() const;

        int dhcpState() const;
        
        bool editableInterfaces() const;

        bool hasEmptyIps() const;

        QObject *itfSettingsModel();

        ///

        const QString &systemName() const;

        const QString &password() const;

        ///

        QDateTime dateTime() const;

        QObject *timeZonesModel();        

    signals:
        void changed(); /// fake signal

        void systemSettingsChanged();

        void interfaceSettingsChanged();

        void dateTimeSettingsChanged();

    private:
        struct Snapshot;
        typedef QScopedPointer<Snapshot> SnapshotPtr;

        SnapshotPtr m_snapshot;
    };
}
