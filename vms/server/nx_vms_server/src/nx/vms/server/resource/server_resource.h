#pragma once

#include <atomic>
#include <memory>
#include <QtCore/QSharedPointer>
#include <core/resource/media_server_resource.h>

class QnMediaServerModule;

namespace nx::vms::server {

    class MediaServerResource: public QnMediaServerResource
    {
        using base_type = QnMediaServerResource;
    public:
        MediaServerResource(QnCommonModule* commonModule);
        ~MediaServerResource();

        enum class Metrics
        {
            transactions,
            timeChanged,
            decodedPixels,
            encodedPixels,
            ruleActionsTriggered,
        };

        qint64 getAndResetMetric(Metrics parameter);

    private:
        qint64 getDelta(Metrics key, qint64 value);
        void at_syncTimeChanged(qint64 syncTime);
    private:
        mutable QnMutex m_mutex;
        QMap<Metrics, qint64> m_counters;
        int m_timeChangeEvents = 0;
    };

    using MediaServerResourcePtr = QnSharedResourcePointer<MediaServerResource>;

} // namespace nx::vms::server
