
#pragma once

#include <QUuid>
#include <QVector>
#include <QSharedPointer>

#include <base/types.h>

namespace rtu
{
    struct BaseServerInfo;
    
    class ServersFinder : public QObject
    {
        Q_OBJECT
        
    public:
        typedef QSharedPointer<ServersFinder> Holder;
        
        static Holder create();
        
        virtual ~ServersFinder();

    public:
        void waitForServer(const QUuid &id);

    signals:
        void serverAdded(const BaseServerInfo &baseInfo);

        void serverChanged(const BaseServerInfo &baseInfo);

        void serversRemoved(const IDsVector &removed);

        void serverAppeared(const QUuid &id);

        void serverDiscovered(const BaseServerInfo &baseInfo);

    private:
        ServersFinder();
        
    private:
        class Impl;
        
        Impl * const m_impl;
    };
}
