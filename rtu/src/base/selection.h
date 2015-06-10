
#pragma once

#include <QObject>
#include <QDateTime>

#include <base/constants.h>

namespace rtu
{
    class ServersSelectionModel;
    
    class Selection : public QObject
    {
        Q_OBJECT
        
        Q_PROPERTY(int count READ count NOTIFY changed)
        Q_PROPERTY(int port READ port NOTIFY changed)
        Q_PROPERTY(int flags READ flags NOTIFY changed)
        Q_PROPERTY(QString systemName READ systemName NOTIFY changed)
        Q_PROPERTY(QString password READ password NOTIFY changed)
        Q_PROPERTY(QDateTime dateTime READ dateTime NOTIFY changed)
        
        Q_PROPERTY(bool editableInterfaces READ editableInterfaces NOTIFY changed)
        
    public:
        explicit Selection(ServersSelectionModel *selectionModel
            , QObject *parent = nullptr);
        
        virtual ~Selection();
        
    private:
        /// Getters for properties
        
        int count();
        
        int port();
        
        int flags();
        
        QString systemName();

        QString password();

        QDateTime dateTime();
        
        bool editableInterfaces();
        
    signals:
        void changed();
        
    private:
        struct Snapshot;
        typedef QScopedPointer<Snapshot> SnapshotPtr;

        SnapshotPtr m_snapshot;
    };
}
