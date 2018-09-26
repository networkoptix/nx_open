#include "live_analytics_receiver.h"

#include <memory>
#include <utility>

#include <QtCore/QList>

#include <core/dataconsumer/abstract_data_receptor.h>
#include <core/resource/camera_resource.h>
#include <utils/common/long_runable_cleanup.h>
#include <utils/common/threadqueue.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/video_data_packet.h>

namespace nx::client::desktop {

namespace {

static constexpr int kMaximumBufferLength = 5000;

} // namespace

// ------------------------------------------------------------------------------------------------
// LiveAnalyticsReceiver::Private

class LiveAnalyticsReceiver::Private: public QnAbstractMediaDataReceptor
{
    LiveAnalyticsReceiver* const q;

public:
    Private(LiveAnalyticsReceiver* q): q(q) {}
    virtual ~Private() override { releaseArchiveReader(); }

    QnSecurityCamResourcePtr camera() const { return m_camera; }
    void setCamera(const QnSecurityCamResourcePtr& camera);

    QList<QnAbstractCompressedMetadataPtr> takeData();

private:
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    void releaseArchiveReader();

private:
    std::unique_ptr<QnArchiveStreamReader> m_reader;
    QnSecurityCamResourcePtr m_camera;

    mutable QnMutex m_mutex;
    QList<QnAbstractCompressedMetadataPtr> m_buffer;
};

void LiveAnalyticsReceiver::Private::setCamera(const QnSecurityCamResourcePtr& value)
{
    if (m_camera == value)
        return;

    releaseArchiveReader();

    m_camera = value;
    if (!m_camera)
        return;

    m_reader.reset(new QnArchiveStreamReader(m_camera));
    m_reader->addDataProcessor(this);

    auto archiveDelegate = new QnRtspClientArchiveDelegate(m_reader.get());
    archiveDelegate->setStreamDataFilter(vms::api::StreamDataFilter::objectDetection);
    m_reader->setArchiveDelegate(archiveDelegate);
    m_reader->start();
}

QList<QnAbstractCompressedMetadataPtr> LiveAnalyticsReceiver::Private::takeData()
{
    QnMutexLocker lock(&m_mutex);
    QList<QnAbstractCompressedMetadataPtr> result;
    std::swap(result, m_buffer);
    return result;
}

void LiveAnalyticsReceiver::Private::releaseArchiveReader()
{
    if (!m_reader)
        return;

    m_reader->removeDataProcessor(this);

    if (QnLongRunableCleanup::instance())
        QnLongRunableCleanup::instance()->cleanupAsync(std::move(m_reader));
    else
        m_reader.reset();
}

bool LiveAnalyticsReceiver::Private::canAcceptData() const
{
    QnMutexLocker lock(&m_mutex);
    return m_buffer.size() < kMaximumBufferLength;
}

void LiveAnalyticsReceiver::Private::putData(const QnAbstractDataPacketPtr& data)
{
    auto metadata = std::dynamic_pointer_cast<QnAbstractCompressedMetadata>(data);
    if (!metadata)
        return;

    NX_ASSERT(metadata->metadataType == MetadataType::ObjectDetection);
    if (metadata->metadataType != MetadataType::ObjectDetection)
        return;

    QnMutexLocker lock(&m_mutex);
    m_buffer.push_back(metadata);

    if (m_buffer.size() < kMaximumBufferLength)
        return;

    lock.unlock();
    emit q->dataOverflow({});
}

// ------------------------------------------------------------------------------------------------
// LiveAnalyticsReceiver

LiveAnalyticsReceiver::LiveAnalyticsReceiver(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
}

LiveAnalyticsReceiver::~LiveAnalyticsReceiver()
{
}

QnSecurityCamResourcePtr LiveAnalyticsReceiver::camera() const
{
    return d->camera();
}

void LiveAnalyticsReceiver::setCamera(const QnSecurityCamResourcePtr& value)
{
    d->setCamera(value);
}

QList<QnAbstractCompressedMetadataPtr> LiveAnalyticsReceiver::takeData()
{
    return d->takeData();
}

} // namespace nx::client::desktop
