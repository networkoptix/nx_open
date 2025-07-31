// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_display.h"

#include <cassert>

#include <camera/cam_display.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/utils/counter.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/system_context.h>
#include <utils/common/long_runable_cleanup.h>
#include <utils/common/util.h>

using namespace nx::vms::client::desktop;

QnResourceDisplay::QnResourceDisplay(const QnResourcePtr &resource, QObject *parent):
    base_type(parent),
    m_resource(resource)
{
    NX_ASSERT(resource);

    m_mediaResource = resource.dynamicCast<QnMediaResource>();
    m_dataProvider = m_mediaResource->createDataProvider();

    if (m_dataProvider)
    {
        m_archiveReader = dynamic_cast<QnAbstractArchiveStreamReader *>(m_dataProvider.data());
        if (m_archiveReader && resource->flags().testFlag(Qn::local_media))
            m_archiveReader->setCycleMode(true);

        m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider *>(m_dataProvider.data());

        // There exists cases with MediaStreamDataProvider but without ArchiveStreamReader,
        // e.g. local image file.
        if (m_mediaProvider)
        {
            /* Camera will free archive reader in its destructor. */
            m_camDisplay = std::make_unique<QnCamDisplay>(m_mediaResource, m_archiveReader);

            m_mediaProvider->addDataProcessor(m_camDisplay.get());
        }
    }

    if (m_camDisplay && m_archiveReader)
    {
        connect(m_archiveReader, &QnAbstractArchiveStreamReader::streamAboutToBePaused,
            m_camDisplay.get(), &QnCamDisplay::onReaderPaused, Qt::DirectConnection);
        connect(m_archiveReader, &QnAbstractArchiveStreamReader::streamAboutToBeResumed,
            m_camDisplay.get(), &QnCamDisplay::onReaderResumed, Qt::DirectConnection);
        connect(m_archiveReader, &QnAbstractArchiveStreamReader::prevFrameOccurred,
            m_camDisplay.get(), &QnCamDisplay::onPrevFrameOccurred, Qt::DirectConnection);
        connect(m_archiveReader, &QnAbstractArchiveStreamReader::nextFrameOccurred,
            m_camDisplay.get(), &QnCamDisplay::onNextFrameOccurred, Qt::DirectConnection);
        connect(m_archiveReader, &QnAbstractArchiveStreamReader::beforeJump,
            m_camDisplay.get(), &QnCamDisplay::onBeforeJump, Qt::DirectConnection);
        connect(m_archiveReader, &QnAbstractArchiveStreamReader::jumpOccurred,
            m_camDisplay.get(), &QnCamDisplay::onJumpOccurred, Qt::DirectConnection);
        connect(m_archiveReader, &QnAbstractArchiveStreamReader::skipFramesTo,
            m_camDisplay.get(), &QnCamDisplay::onSkippingFrames, Qt::DirectConnection);
    }
}

QnResourceDisplay::~QnResourceDisplay()
{
    if (m_mediaProvider && m_camDisplay)
        m_mediaProvider->removeDataProcessor(m_camDisplay.get());

    const bool hasCamDisplay = !!m_camDisplay;
    const bool hasArchiveReader = !m_archiveReader.isNull();

    // The cam display should stop and release the archive reader.
    if (hasCamDisplay)
        cleanUp(m_camDisplay.release());

    if ((!hasCamDisplay || !hasArchiveReader) && m_dataProvider)
        cleanUp(m_dataProvider);
}

void QnResourceDisplay::beforeDestroy()
{
}

const QnResourcePtr& QnResourceDisplay::resource() const
{
    return m_resource;
}

const QnMediaResourcePtr& QnResourceDisplay::mediaResource() const
{
    return m_mediaResource;
}

QnAbstractStreamDataProvider* QnResourceDisplay::dataProvider() const
{
    return m_dataProvider.data();
}

QnAbstractMediaStreamDataProvider* QnResourceDisplay::mediaProvider() const
{
    return m_mediaProvider.data();
}

QnAbstractArchiveStreamReader* QnResourceDisplay::archiveReader() const
{
    return m_archiveReader.data();
}

void QnResourceDisplay::cleanUp(QnLongRunnable* runnable) const
{
    if (runnable == nullptr)
        return;

    auto context = m_resource ? m_resource->systemContext() : nullptr;
    nx::vms::common::appContext()->longRunableCleanup()->cleanupAsync(
        std::unique_ptr<QnLongRunnable>(runnable), context);
}

QnCamDisplay *QnResourceDisplay::camDisplay() const
{
    return m_camDisplay.get();
}

QnConstResourceVideoLayoutPtr QnResourceDisplay::videoLayout() const {
    if (!m_mediaProvider)
        return QnConstResourceVideoLayoutPtr();

    if (!m_mediaResource)
        return QnConstResourceVideoLayoutPtr();

    return m_mediaResource->getVideoLayout(m_mediaProvider);
}

qint64 QnResourceDisplay::currentTimeUSec() const
{
    if (!m_camDisplay)
        return -1;

    if (m_camDisplay->isRealTimeSource())
        return DATETIME_NOW;

    return m_camDisplay->getCurrentTime();
}

void QnResourceDisplay::start()
{
    NX_VERBOSE(this, "Start for resource: %1", m_resource);

    if (m_camDisplay)
        m_camDisplay->start();

    if (m_mediaProvider)
        m_mediaProvider->start(QThread::HighPriority);
}

void QnResourceDisplay::play() {
    if(m_archiveReader == nullptr)
        return;

    m_archiveReader->resumeMedia();
}

void QnResourceDisplay::pause() {
    if (!m_archiveReader)
        return;

    m_archiveReader->pauseMedia();
}

bool QnResourceDisplay::isPaused() {
    if (!m_archiveReader)
        return false;

    return m_archiveReader->isMediaPaused();
}

bool QnResourceDisplay::isStillImage() const {
    return m_resource->flags() & Qn::still_image;
}

void QnResourceDisplay::addRenderer(QnResourceWidgetRenderer *renderer) {
    if (m_camDisplay)
        m_camDisplay->addVideoRenderer(videoLayout()->channelCount(), renderer, true);
}

void QnResourceDisplay::removeRenderer(QnResourceWidgetRenderer* renderer)
{
    if (m_camDisplay)
        m_camDisplay->removeVideoRenderer(renderer);
}

void QnResourceDisplay::addMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    if (m_camDisplay)
        m_camDisplay->addMetadataConsumer(metadataConsumer);
}

void QnResourceDisplay::removeMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    if (m_camDisplay)
        m_camDisplay->removeMetadataConsumer(metadataConsumer);
}

QString QnResourceDisplay::codecName() const
{
    return m_camDisplay ? m_camDisplay->codecName() : QString();
}
