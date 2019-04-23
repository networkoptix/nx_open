#pragma once

#include <QtGui/QImage>

#include <deque>
#include <utils/media/frame_info.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API VideoCache: public QObject
{
public:
    static const std::chrono::microseconds kNoTimestamp;

    VideoCache(QObject* parent = nullptr);

    /*
     * @brief Setup resource list to cache.
     */
    void setCachedDevices(QSet<QnUuid>& value);

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
        std::chrono::microseconds* outImageTimestamp = nullptr);

    void add(const QnUuid& resourceId, const CLVideoDecoderOutputPtr& frame);

    void setCacheSize(std::chrono::microseconds value);
    std::chrono::microseconds cacheSize() const;
private:
    QnMutex m_mutex;
    QMap<QnUuid, std::deque<CLVideoDecoderOutputPtr>> m_cache;
    std::chrono::microseconds m_cacheSize{};
};

} // namespace nx::vms::client::desktop
