
#pragma once

#include <QUuid>
#include <QVector>
#include <QSharedPointer>

#include <base/types.h>

namespace nx { namespace vms::server { namespace api {
    struct BaseServerInfo;
}}}

namespace rtu
{
    class ServersFinder : public QObject
    {
        Q_OBJECT
        
    public:
        typedef QSharedPointer<ServersFinder> Holder;
        
        static Holder create();
        
        virtual ~ServersFinder();

    public:
        void waitForServer(const QUuid &id);

        typedef nx::vms::server::api::BaseServerInfo BaseServerInfo;

    signals:
        void serverAdded(const BaseServerInfo &baseInfo);

        void serverChanged(const BaseServerInfo &baseInfo);

        void serversRemoved(const IDsVector &removed);

        void serverAppeared(const QUuid &id);

        void serverDiscovered(const BaseServerInfo &baseInfo);

        void unknownAdded(const QString &ip);

        void unknownRemoved(const QString &ip);

    private:
        ServersFinder();
        
    private:
        class Impl;
        
        Impl * const m_impl;
    };
}
