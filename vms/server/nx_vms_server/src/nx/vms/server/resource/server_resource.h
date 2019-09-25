#pragma once

#include <atomic>
#include <memory>
#include <QtCore/QSharedPointer>
#include <core/resource/media_server_resource.h>

class QnMediaServerModule;

namespace nx::vms::server {

    class MediaServerResource: public QnMediaServerResource
    {
    public:
        using QnMediaServerResource::QnMediaServerResource;

        enum class Metrics
        {
            transactions,
        };

        qint64 getAndResetMetric(Metrics parameter);

    private:
        qint64 getDelta(Metrics key, qint64 value);
    private:
        QMap<Metrics, qint64> m_counters;
    };

    using MediaServerResourcePtr = QnSharedResourcePointer<MediaServerResource>;

} // namespace nx::vms::server
