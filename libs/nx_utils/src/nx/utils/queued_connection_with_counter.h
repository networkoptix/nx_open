#pragma once

#include <QtCore/QObject>
#include <list>
#include <nx/utils/thread/wait_condition.h>

namespace nx::utils {

/**
 * Wraps QT queued connection. It helps to wait that pending(queued) slots is called.
*/
class NX_UTILS_API QueuedConnectionWithCounter
{
public:
    ~QueuedConnectionWithCounter();

    template <typename SignalType, typename SlotType>
    void queuedConnect(
        typename QtPrivate::FunctionPointer<SignalType>::Object* sender,
        SignalType signal,
        QObject* receiver,
        SlotType slot)
    {
        auto connection = QObject::connect(sender, signal,
            [this, receiver, slot](auto ... args)
            {
                addPendingOp();
                QMetaObject::invokeMethod(receiver,
                    [this, slot, args...]()
                    {
                        slot(args...);
                        finishPendingOp();
                    },
                    Qt::QueuedConnection);
            });
        m_connections.push_back(std::make_pair(sender, connection));
    }

    void queuedDisconnectAll();

    /**
     * If some signals already triggered, but queued slots is not finished yet this function wait until pending slots
     * to be called and finished.
     */
    void waitForPendingSlotsToBeFinished();

private:
    void addPendingOp();
    void finishPendingOp();

private:
    mutable QnMutex m_pendingOpMutex;
    mutable QnWaitCondition m_pendingOpWaitCondition;
    int m_pendingOpCounter = 0;
    std::list<int> m_expectants;
    std::list<std::pair<QObject*, QMetaObject::Connection>> m_connections;
};

} // namespace nx::utils
