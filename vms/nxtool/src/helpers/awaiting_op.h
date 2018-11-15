
#pragma once

#include <memory>
#include <functional>

#include <QUuid>
#include <QObject>

#include <base/types.h>

namespace nx { namespace mediaserver { namespace api {
    struct BaseServerInfo;
}}}

namespace rtu
{
    class AwaitingOp
    {
    public:
        typedef nx::mediaserver::api::BaseServerInfo BaseServerInfo;

        typedef std::shared_ptr<AwaitingOp> Holder;
        typedef std::weak_ptr<AwaitingOp> WeakPtr;
        typedef std::function<void (const WeakPtr op)> TimeoutHandler;
        typedef std::function<bool (const QString &ip)> UnknownAddedHandler;
        typedef std::function<void (const BaseServerInfo &info)> ServerDiscoveredAction;

        static Holder create(const QUuid &id
            , int changesCount
            , qint64 timeout
            , const TimeoutHandler &timeoutHandler);

        virtual ~AwaitingOp();

        void setServerDiscoveredHandler(const ServerDiscoveredAction &handler);

        void setServerDisappearedHandler(const Callback &handler);

        void setUnknownAddedHandler(const UnknownAddedHandler &handler);

        void processServerDiscovered(const BaseServerInfo &info);

        void processServerDisappeared();

        bool processUnknownAdded(const QString &ip);

        const QUuid &serverId() const;

        int changesCount() const;

        void resetChangesCount();

    private:
        AwaitingOp(const QUuid &id
            , int changesCount
            , qint64 timeout);

        bool isTimedOut() const;

    private:
        const QUuid m_serverId;
        const qint64 m_creationTimestamp;
        const qint64 m_timeout;

        int m_changesCount;
        ServerDiscoveredAction m_discovered;
        Callback m_disappeared;
        UnknownAddedHandler m_unknownAdded;

    };
}
