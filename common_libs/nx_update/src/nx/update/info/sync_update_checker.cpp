#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/log/log.h>
#include "sync_update_checker.h"

namespace nx {
namespace update {
namespace info {

namespace {
class SyncUpdateChecker
{
public:
    SyncUpdateChecker(
        const QnUuid& selfPeerId,
        const QString& url)
    {
        using namespace std::placeholders;
        m_asyncUpdateChecker.check(
            std::bind(&SyncUpdateChecker::onUpdateDone, this, _1, _2),
            selfPeerId,
            url);
    }

    AbstractUpdateRegistryPtr take()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_done)
            m_waitCondition.wait(lock.mutex());
        return std::move(m_abstractUpdateRegistryPtr);
    }
private:
    AsyncUpdateChecker m_asyncUpdateChecker;
    AbstractUpdateRegistryPtr m_abstractUpdateRegistryPtr;
    QnMutex m_mutex;
    QnWaitCondition m_waitCondition;
    bool m_done = false;

    void onUpdateDone(ResultCode resultCode, AbstractUpdateRegistryPtr abstractUpdateRegistryPtr)
    {
        QnMutexLocker lock(&m_mutex);

        if (resultCode != ResultCode::ok)
            NX_WARNING(this, lm("Failed to get updates info: %1").args(toString(resultCode)));
        else
            m_abstractUpdateRegistryPtr = std::move(abstractUpdateRegistryPtr);

        m_done = true;
        m_waitCondition.wakeOne();
    }
};
} // namespace

AbstractUpdateRegistryPtr checkSync(const QnUuid& selfPeerId, const QString& url)
{
    return SyncUpdateChecker(selfPeerId, url).take();
}

} // namespace info
} // namespace update
} // namespace nx
