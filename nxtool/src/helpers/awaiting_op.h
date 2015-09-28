
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
        typedef std::function<bool (const QString &ip)> UnknownAddedHandler;
        typedef std::function<void (const BaseServerInfo &info)> ServerDiscoveredAction;

        static Holder create(const QUuid &id
            , int changesCount
            , qint64 timeout
            , const ServerDiscoveredAction &discovered
            , const Callback &disappeared
            , const UnknownAddedHandler &unknownAdded
            , const TimeoutHandler &timeoutHandler);

        virtual ~AwaitingOp();

        void processServerDiscovered(const BaseServerInfo &info);

        void processServersDisappeared();

        bool processUnknownAdded(const QString &ip);

        const QUuid &id() const;

        int changesCount() const;

    signals:
        void timeout();

    private:
        AwaitingOp(const QUuid &id
            , int changesCount
            , qint64 timeout
            , const ServerDiscoveredAction &discovered
            , const UnknownAddedHandler &unknownAdded
            , const Callback &disappeared);

        bool isTimedOut() const;

    private:
        const QUuid m_id;
        const ServerDiscoveredAction m_discovered;
        const Callback m_disappeared;
        const UnknownAddedHandler m_unknownAdded;
        const int m_changesCount;
        const qint64 m_creationTimestamp;
        const qint64 m_timeout;
    };
}
