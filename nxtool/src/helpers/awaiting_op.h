
#pragma once

#include <memory>
#include <functional>

#include <Quuid>
#include <QObject>

#include <base/types.h>

namespace rtu
{
    struct BaseServerInfo;

    class AwaitingOp
    {
    public:
        typedef std::shared_ptr<AwaitingOp> Holder;
        typedef std::function<void (const QUuid &id)> TimeoutHandler;
        typedef std::function<void (const IDsVector &ids)> ServersDisappearedAction;
        typedef std::function<void (const BaseServerInfo &info)> ServerDiscoveredAction;

        static Holder create(const QUuid &id
            , int changesCount
            , qint64 timeout
            , const ServerDiscoveredAction &discovered
            , const ServersDisappearedAction &disappeared
            , const TimeoutHandler &timeoutHandler);

        virtual ~AwaitingOp();

        void serverDiscovered(const BaseServerInfo &info);

        void serversDisappeared(const IDsVector &ids);

        const QUuid &id() const;

        int changesCount() const;

    signals:
        void timeout();

    private:
        AwaitingOp(const QUuid &id
            , int changesCount
            , qint64 timeout
            , const ServerDiscoveredAction &discovered
            , const ServersDisappearedAction &disappeared);

        bool isTimedOut() const;

    private:
        const QUuid m_id;
        const ServerDiscoveredAction m_discovered;
        const ServersDisappearedAction m_disappeared;
        const int m_changesCount;
        const qint64 m_creationTimestamp;
        const qint64 m_timeout;
    };
}
