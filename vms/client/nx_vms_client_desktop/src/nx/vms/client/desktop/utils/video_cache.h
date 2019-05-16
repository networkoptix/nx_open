#pragma once

#include <chrono>
#include <deque>

#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QObject>
#include <QtGui/QImage>

#include <utils/media/frame_info.h>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API VideoCache: public QObject
{
public:
    static const std::chrono::microseconds kNoTimestamp;

    VideoCache(QObject* parent = nullptr);

    /*
     * @brief Setup resource list to cache.
     */
    void setCachedDevices(intptr_t consumerId, const QSet<QnUuid>& value);

    /*
     * @brief Find image in the video cache.
     * @param resourceId Device to search.
     * @param timestamp. Cache returns the nearest frame with value >= timestamp. Or null QImage if not found.
     * @param outImageTimestamp. Found frame timestamp or -1 if not found.
     * @return Cached image or empty QImage if not found.
     */
    QImage image(
        const QnUuid& resourceId,
        std::chrono::microseconds timestamp,
        std::chrono::microseconds* outImageTimestamp = nullptr) const;

    void add(const QnUuid& resourceId, const CLVideoDecoderOutputPtr& frame);

    void setCacheSize(std::chrono::microseconds value);
    std::chrono::microseconds cacheSize() const;

private:
    void setCachedDevices(const QSet<QnUuid>& value);

private:
    mutable QnMutex m_mutex;
    QMap<intptr_t, QSet<QnUuid>> m_cachedDevices;
    QMap<QnUuid, std::deque<CLVideoDecoderOutputPtr>> m_cache;
    std::chrono::microseconds m_cacheSize{};
};

} // namespace nx::vms::client::desktop
