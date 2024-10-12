// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtGui/QImage>

#include <nx/media/ffmpeg/frame_info.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API VideoCache: public QObject
{
public:
    static const std::chrono::microseconds kNoTimestamp;

    VideoCache(QObject* parent = nullptr);

    /*
     * @brief Setup resource list to cache.
     */
    void setCachedDevices(intptr_t consumerId, const QSet<nx::Uuid>& value);

    /*
     * @brief Find image in the video cache.
     * @param resourceId Device to search.
     * @param timestamp. Cache returns the nearest frame with value >= timestamp. Or null QImage if not found.
     * @param outImageTimestamp. Found frame timestamp or -1 if not found.
     * @return Cached image or empty QImage if not found.
     */
    QImage image(
        const nx::Uuid& resourceId,
        std::chrono::microseconds timestamp,
        std::chrono::microseconds* outImageTimestamp = nullptr) const;

    void add(const nx::Uuid& resourceId, const CLConstVideoDecoderOutputPtr& frame);

    void setCacheSize(std::chrono::microseconds value);
    std::chrono::microseconds cacheSize() const;

private:
    void setCachedDevices(const QSet<nx::Uuid>& value);

private:
    mutable nx::Mutex m_mutex;
    QMap<intptr_t, QSet<nx::Uuid>> m_cachedDevices;
    QMap<nx::Uuid, std::deque<CLConstVideoDecoderOutputPtr>> m_cache;
    std::chrono::microseconds m_cacheSize{};
};

} // namespace nx::vms::client::core
