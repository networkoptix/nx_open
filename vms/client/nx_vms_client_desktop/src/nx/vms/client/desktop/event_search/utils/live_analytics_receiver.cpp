#include "live_analytics_receiver.h"

#include <memory>
#include <utility>

#include <core/dataconsumer/abstract_data_receptor.h>
#include <core/resource/camera_resource.h>
#include <utils/common/long_runable_cleanup.h>
#include <utils/common/threadqueue.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMaximumBufferLength = 1000;

} // namespace

// ------------------------------------------------------------------------------------------------
// LiveAnalyticsReceiver::Private

class LiveAnalyticsReceiver::Private: public QnAbstractMediaDataReceptor
{
    LiveAnalyticsReceiver* const q;

public:
    Private(LiveAnalyticsReceiver* q): q(q) {}
    virtual ~Private() override { releaseArchiveReader(); }

    QnVirtualCameraResourcePtr camera() const { return m_camera; }
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    QList<QnAbstractCompressedMetadataPtr> takeData();

private:
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    void releaseArchiveReader();

private:
    std::unique_ptr<QnArchiveStreamReader> m_reader;
    QnVirtualCameraResourcePtr m_camera;

    mutable QnMutex m_mutex;
    QList<QnAbstractCompressedMetadataPtr> m_buffer;
};

void LiveAnalyticsReceiver::Private::setCamera(const QnVirtualCameraResourcePtr& value)
{
    if (m_camera == value)
        return;

    releaseArchiveReader();

    m_camera = value;
    if (!m_camera)
        return;

    m_reader.reset(new QnArchiveStreamReader(m_camera));
    m_reader->setArchiveDelegate(new QnRtspClientArchiveDelegate(m_reader.get()));
    m_reader->setStreamDataFilter(vms::api::StreamDataFilter::objectDetection);
    m_reader->addDataProcessor(this);
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

LiveAnalyticsReceiver::LiveAnalyticsReceiver(
    const QnVirtualCameraResourcePtr& camera, QObject* parent)
    :
    LiveAnalyticsReceiver(parent)
{
    setCamera(camera);
}

LiveAnalyticsReceiver::~LiveAnalyticsReceiver()
{
}

QnVirtualCameraResourcePtr LiveAnalyticsReceiver::camera() const
{
    return d->camera();
}

void LiveAnalyticsReceiver::setCamera(const QnVirtualCameraResourcePtr& value)
{
    d->setCamera(value);
}

QList<QnAbstractCompressedMetadataPtr> LiveAnalyticsReceiver::takeData()
{
    return d->takeData();
}

} // namespace nx::vms::client::desktop
