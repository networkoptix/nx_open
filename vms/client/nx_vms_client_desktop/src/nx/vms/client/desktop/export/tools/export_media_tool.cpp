// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_media_tool.h"

#include <QtCore/QFileInfo>

#include <client_core/client_core_module.h>
#include <core/dataprovider/data_provider_factory.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/avi/thumbnails_stream_reader.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <media/filters/h264_mp4_to_annexb.h>
#include <nx/network/http/custom_headers.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <nx/vms/client/desktop/export/tools/export_timelapse_recorder.h>
#include <nx/vms/client/desktop/utils/timezone_helper.h>

namespace nx::vms::client::desktop {

namespace {

QString container(FileExtension ext)
{
    switch (ext)
    {
        case FileExtension::avi:
            return lit("avi");
        case FileExtension::mkv:
            return lit("mkv");
        case FileExtension::mp4:
            return lit("mp4");
        default:
            NX_ASSERT(false, "Should never get here");
            return lit("mkv");
    }
}

} // namespace

struct ExportMediaTool::Private
{
    ExportMediaTool* const q;
    const ExportMediaSettings settings;
    QnMediaResourcePtr mediaResource;
    QScopedPointer<QnAbstractMediaStreamDataProvider> dataProvider;
    QScopedPointer<ExportStorageStreamRecorder> exportRecorder;
    ExportProcessError lastError = ExportProcessError::noError;
    ExportProcessStatus status = ExportProcessStatus::initial;

    explicit Private(ExportMediaTool* owner, const ExportMediaSettings& settings, QnMediaResourcePtr mediaResource):
        q(owner),
        settings(settings),
        mediaResource(mediaResource)
    {
    }

    bool exportMediaPeriodToFile()
    {
        NX_ASSERT(status == ExportProcessStatus::initial);
        const auto timelapseFrameStepUs = settings.timelapseFrameStepMs * 1000ll;
        const auto startTimeUs = settings.period.startTimeMs * 1000ll;
        NX_ASSERT(settings.period.durationMs > 0,
            "Invalid time period, possibly LIVE is exported");
        const auto endTimeUs = settings.period.durationMs > 0
            ? settings.period.endTimeMs() * 1000ll
            : DATETIME_NOW;

        if (!initDataProvider(startTimeUs, endTimeUs, timelapseFrameStepUs))
        {
            setStatus(ExportProcessStatus::failure);
            return false;
        }

        connect(dataProvider.data(), &QnAbstractArchiveStreamReader::finished, q,
            [this]
            {
                finishExport();
            });

        if (!initExportRecorder(timelapseFrameStepUs, dataProvider.get()))
        {
            setStatus(ExportProcessStatus::failure);
            return false;
        }

        exportRecorder->setPreciseStartPosition(startTimeUs);

        connect(exportRecorder.data(), &QnStreamRecorder::finished, q,
            [this]()
            {
                lastError = convertError(exportRecorder->getLastError());

                dataProvider->removeDataProcessor(exportRecorder.data());
                dataProvider->pleaseStop();
            });

        nx::core::transcoding::FilterChain filters(
            settings.transcodingSettings,
            mediaResource->getDewarpingParams(),
            mediaResource->getVideoLayout());
        exportRecorder->setTranscodeFilters(filters);

        connect(exportRecorder.get(), &QnStreamRecorder::recordingProgress, q,
            &ExportMediaTool::valueChanged);

        connect(exportRecorder.get(), &QnStreamRecorder::recordingFinished, q,
            [this](const std::optional<nx::recording::Error>& /*status*/, const QString& /*fileName*/)
            {
                emit q->valueChanged(100);
            });

        auto archiveReader = dynamic_cast<QnAbstractArchiveStreamReader*>(dataProvider.data());

        exportRecorder->clearUnprocessedData();
        exportRecorder->setProgressBounds(startTimeUs, endTimeUs);

        if (!exportRecorder->addRecordingContext(settings.fileName.completeFileName()))
        {
            setStatus(ExportProcessStatus::failure);
            return false;
        }

        if (!timelapseFrameStepUs)
            exportRecorder->setServerTimeZone(timeZone(mediaResource));

        exportRecorder->setContainer(container(settings.fileName.extension));

        dataProvider->addDataProcessor(exportRecorder.data());
        if (archiveReader)
            archiveReader->jumpTo(startTimeUs, startTimeUs);

        dataProvider->start();
        exportRecorder->start();

        setStatus(ExportProcessStatus::exporting);
        return true;
    }

    void cancelExport()
    {
        // Double cancel is ok.
        if (status == ExportProcessStatus::cancelling)
            return;

        NX_ASSERT(status == ExportProcessStatus::exporting);
        setStatus(ExportProcessStatus::cancelling);

        // Minor optimization.
        dataProvider->removeDataProcessor(exportRecorder.data());
        //dataProvider->pleaseStop(); //< Will be stoped after export recorder stop.

        exportRecorder->pleaseStop();
    }

private:
    void setStatus(ExportProcessStatus value)
    {
        NX_ASSERT(status != value);
        status = value;
        emit q->statusChanged(status);
    }

    bool initDataProvider(qint64 startTimeUs, qint64 endTimeUs, qint64 timelapseFrameStepUs)
    {
        NX_ASSERT(!dataProvider);
        const bool isRapidReview = timelapseFrameStepUs > 0;

        if (mediaResource->toResource()->hasFlags(Qn::local) && isRapidReview)
        {
            auto thumbnailsReader = new QnThumbnailsStreamReader(
                mediaResource->toResourcePtr(),
                new QnAviArchiveDelegate());
            thumbnailsReader->setRange(startTimeUs, endTimeUs, timelapseFrameStepUs, 0);
            dataProvider.reset(thumbnailsReader);
            return true;
        }

        const auto tmpReader = qnClientCoreModule->dataProviderFactory()->createDataProvider(
                mediaResource->toResourcePtr());

        auto archiveReader = dynamic_cast<QnAbstractArchiveStreamReader*>(tmpReader);
        if (!archiveReader)
        {
            delete tmpReader;
            lastError = ExportProcessError::unsupportedMedia;
            return false;
        }

        archiveReader->setCycleMode(false);
        archiveReader->setQuality(MEDIA_Quality_ForceHigh, true);
        // Additing filtering is required in case of.AVI export.
        archiveReader->addMediaFilter(std::make_shared<H2645Mp4ToAnnexB>());

        if (auto rtspClient = dynamic_cast<QnRtspClientArchiveDelegate*>
            (archiveReader->getArchiveDelegate()))
        {
            // 'Slow' open mode. Send DESCRIBE and SETUP to server. It is required for av_streams
            // in output file - we should know all codec context immediately.
            const auto camera = mediaResource->toResourcePtr()
                .dynamicCast<QnVirtualCameraResource>();
            NX_ASSERT(camera);
            rtspClient->setCamera(camera);
            rtspClient->setPlayNowModeAllowed(false);
            rtspClient->setMediaRole(PlaybackMode::export_);
            rtspClient->setRange(startTimeUs, endTimeUs, timelapseFrameStepUs);
        }

        dataProvider.reset(archiveReader);
        return true;
    }

    bool initExportRecorder(qint64 timelapseFrameStepUs, QnAbstractMediaStreamDataProvider* mediaProvider)
    {
        NX_ASSERT(!exportRecorder);
        const auto resource = mediaResource->toResourcePtr();
        if (timelapseFrameStepUs > 0)
            exportRecorder.reset(new ExportTimelapseRecorder(resource, mediaProvider, timelapseFrameStepUs));
        else
            exportRecorder.reset(new ExportStorageStreamRecorder(resource, mediaProvider));
        return true;
    }

    void finishExport()
    {
        switch (status)
        {
            case ExportProcessStatus::exporting:
                setStatus(lastError == ExportProcessError::noError
                    ? ExportProcessStatus::success
                    : ExportProcessStatus::failure);
                break;
            case ExportProcessStatus::cancelling:
                setStatus(ExportProcessStatus::cancelled);
                break;
            default:
                NX_ASSERT(false, "Should never get here");
                break;
        }
        if (status != ExportProcessStatus::success)
            QFile::remove(settings.fileName.completeFileName());
        emit q->finished();
    }

};

ExportMediaTool::ExportMediaTool(
    const ExportMediaSettings& settings,
    QnMediaResourcePtr mediaResource,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, settings, mediaResource))
{
}

ExportMediaTool::~ExportMediaTool()
{
}

bool ExportMediaTool::start()
{
    emit rangeChanged(0, 100);
    emit valueChanged(0);
    return d->exportMediaPeriodToFile();
}

ExportProcessError ExportMediaTool::lastError() const
{
    return d->lastError;
}

ExportProcessStatus ExportMediaTool::processStatus() const
{
    return d->status;
}

void ExportMediaTool::stop()
{
    d->cancelExport();
}

} // namespace nx::vms::client::desktop
