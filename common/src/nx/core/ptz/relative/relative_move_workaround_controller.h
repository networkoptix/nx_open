#pragma once

#include <map>
#include <memory>

#include <QtCore/QThreadPool>

#include <core/ptz/proxy_ptz_controller.h>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>

#include <nx/core/ptz/relative/relative_move_engine.h>
#include <nx/core/ptz/sequence_maker.h>
#include <nx/core/ptz/sequence_executor.h>

namespace nx {
namespace core {
namespace ptz {

class RelativeMoveWorkaroundController: public QnProxyPtzController
{
    using base_type = QnProxyPtzController;

    class CallbackTrigger
    {
    public:
        CallbackTrigger(RelativeMoveDoneCallback callback, int counter):
            m_counter(counter),
            m_callback(callback)
        {
        }

        bool trigger()
        {
            RelativeMoveDoneCallback callback;
            {
                QnMutexLocker lock(&m_mutex);
                --m_counter;
                if (m_callback && m_counter == 0)
                    m_callback.swap(callback);
                else
                    return false;
            }

            if (callback)
                callback();
            return true;
        }
    private:
        QnMutex m_mutex;
        int m_counter = 1;
        RelativeMoveDoneCallback m_callback;
    };

public:
    RelativeMoveWorkaroundController(
        const QnPtzControllerPtr& controller,
        const std::shared_ptr<AbstractSequenceMaker>& sequenceMaker,
        const std::shared_ptr<AbstractSequenceExecutor>& sequenceExecutor);

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(
        const nx::core::ptz::Options& options) const override;

    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(
        qreal direction,
        const nx::core::ptz::Options& options) override;

    void setRelativeMoveDoneCallback(RelativeMoveDoneCallback callback);

private:
    Ptz::Capability extendsWith(
        Ptz::Capability relativeMoveCapability,
        const nx::core::ptz::Options& options) const;

    QnUuid createTrigger(RelativeMoveDoneCallback callback, int engineCount);
    void trigger(const QnUuid& id);

private:
    mutable QnMutex m_triggerMutex;
    std::unique_ptr<RelativeMoveEngine> m_absoluteMoveEngine;
    std::unique_ptr<RelativeMoveEngine> m_continuousMoveEngine;
    RelativeMoveDoneCallback m_relativeMoveDoneCallback;
    std::map<QnUuid, std::shared_ptr<CallbackTrigger>> m_callbackTriggers;

};

} // namespace ptz
} // namespace core
} // namespace nx
