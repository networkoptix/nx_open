#include "export_media_tool.h"

#include <QtCore/QFileInfo>

#include <client_core/client_core_module.h>

#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/dataprovider/data_provider_factory.h>

#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <nx/vms/client/desktop/export/tools/export_timelapse_recorder.h>

#include <nx/network/http/custom_headers.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>

#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/avi/thumbnails_stream_reader.h>
#include <media/filters/h264_mp4_to_annexb.h>

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
    QScopedPointer<QnAbstractMediaStreamDataProvider> dataProvider;
    QScopedPointer<QnStreamRecorder> exportRecorder;
    ExportProcessError lastError = ExportProcessError::noError;
    ExportProcessStatus status = ExportProcessStatus::initial;

    explicit Private(ExportMediaTool* owner, const ExportMediaSettings& settings):
        q(owner),
        settings(settings)
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

        if (!initExportRecorder(timelapseFrameStepUs))
        {
            setStatus(ExportProcessStatus::failure);
            return false;
        }

        connect(exportRecorder.data(), &QnStreamRecorder::finished, q,
            [this]()
            {
                dataProvider->removeDataProcessor(exportRecorder.data());
                dataProvider->pleaseStop();
            });

        nx::core::transcoding::FilterChain filters(settings.transcodingSettings);
        exportRecorder->setTranscodeFilters(filters);

        connect(exportRecorder, &QnStreamRecorder::recordingProgress, q,
            &ExportMediaTool::valueChanged);

        connect(exportRecorder, &QnStreamRecorder::recordingFinished, q,
            [this](const StreamRecorderErrorStruct &status, const QString& /*fileName*/)
            {
                lastError = convertError(status.lastError);
                emit q->valueChanged(100);
            });

        if (settings.fileName.extension == FileExtension::avi
            || settings.fileName.extension == FileExtension::mp4)
        {
            exportRecorder->setAudioCodec(AV_CODEC_ID_MP2); //< Transcode audio to MP2.
        }

        auto archiveReader = dynamic_cast<QnAbstractArchiveStreamReader*>(dataProvider.data());

        exportRecorder->clearUnprocessedData();
        exportRecorder->setProgressBounds(startTimeUs, endTimeUs);
        exportRecorder->addRecordingContext(settings.fileName.completeFileName());

        exportRecorder->setRole(StreamRecorderRole::fileExport);
        if (!timelapseFrameStepUs)
            exportRecorder->setServerTimeZoneMs(settings.serverTimeZoneMs);
        exportRecorder->setContainer(container(settings.fileName.extension));
        exportRecorder->setNeedCalcSignature(true);

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
        NX_ASSERT(status == ExportProcessStatus::exporting);
        setStatus(ExportProcessStatus::cancelling);

        // Minor optimization.
        dataProvider->removeDataProcessor(exportRecorder.data());
        //dataProvider->pleaseStop(); //< Will be stoped after export recorder stop.

        // Clean signature flag; in other case file will be recreated on writing finish.
        // TODO: #vasilenko get rid of this magic
        exportRecorder->setNeedCalcSignature(false);
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

        if (settings.mediaResource->toResource()->hasFlags(Qn::local) && isRapidReview)
        {
            auto thumbnailsReader = new QnThumbnailsStreamReader(
                settings.mediaResource->toResourcePtr(), new QnAviArchiveDelegate());
            thumbnailsReader->setRange(startTimeUs, endTimeUs, timelapseFrameStepUs, 0);
            dataProvider.reset(thumbnailsReader);
            return true;
        }

        const auto tmpReader = qnClientCoreModule->dataProviderFactory()->createDataProvider(
                settings.mediaResource->toResourcePtr());

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
        archiveReader->addMediaFilter(std::make_shared<H264Mp4ToAnnexB>());

        if (auto rtspClient = dynamic_cast<QnRtspClientArchiveDelegate*>
            (archiveReader->getArchiveDelegate()))
        {
            // 'Slow' open mode. Send DESCRIBE and SETUP to server. It is required for av_streams
            // in output file - we should know all codec context immediately.
            const auto camera = settings.mediaResource->toResourcePtr()
                .dynamicCast<QnVirtualCameraResource>();
            NX_ASSERT(camera);
            rtspClient->setCamera(camera);
            rtspClient->setPlayNowModeAllowed(false);
            rtspClient->setAdditionalAttribute(Qn::EC2_MEDIA_ROLE, "export");
            if (isRapidReview)
                rtspClient->setRange(startTimeUs, endTimeUs, timelapseFrameStepUs);
        }

        dataProvider.reset(archiveReader);
        return true;
    }

    bool initExportRecorder(qint64 timelapseFrameStepUs)
    {
        NX_ASSERT(!exportRecorder);
        const auto resource = settings.mediaResource->toResourcePtr();
        if (timelapseFrameStepUs > 0)
            exportRecorder.reset(new ExportTimelapseRecorder(resource, timelapseFrameStepUs));
        else
            exportRecorder.reset(new QnStreamRecorder(resource));
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
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, settings))
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
