#include "export_media_tool.h"

#include <QtCore/QFileInfo>

#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/client/desktop/export/data/export_media_settings.h>
#include <nx/client/desktop/export/tools/export_timelapse_recorder.h>

#include <nx/network/http/custom_headers.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>

#include <plugins/resource/avi/avi_archive_delegate.h>
#include <plugins/resource/avi/thumbnails_stream_reader.h>

namespace nx {
namespace client {
namespace desktop {

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
    StreamRecorderError status = StreamRecorderError::noError;

    ExportProcessStatus state = ExportProcessStatus::initial;

    explicit Private(ExportMediaTool* owner, const ExportMediaSettings& settings):
        q(owner),
        settings(settings)
    {
    }

    bool exportMediaPeriodToFile()
    {
        NX_ASSERT(state == ExportProcessStatus::initial);
        const auto timelapseFrameStepUs = settings.timelapseFrameStepMs * 1000ll;
        const auto startTimeUs = settings.timePeriod.startTimeMs * 1000ll;
        NX_ASSERT(settings.timePeriod.durationMs > 0,
            "Invalid time period, possibly LIVE is exported");
        const auto endTimeUs = settings.timePeriod.durationMs > 0
            ? settings.timePeriod.endTimeMs() * 1000ll
            : DATETIME_NOW;

        if (!initDataProvider(startTimeUs, endTimeUs, timelapseFrameStepUs))
        {
            state = ExportProcessStatus::failure;
            emit q->statusChanged(state);
            return false;
        }

        connect(dataProvider.data(), &QnAbstractArchiveStreamReader::finished, q,
            [this]
            {
                qDebug() << "dataProvider finished in thread" << QThread::currentThreadId();
                finishExport();
            });

        if (!initExportRecorder(timelapseFrameStepUs))
        {
            state = ExportProcessStatus::failure;
            emit q->statusChanged(state);
            return false;
        }

        connect(exportRecorder.data(), &QnStreamRecorder::finished, q,
            [this]()
            {
                qDebug() << "exportRecorder finished in thread" << QThread::currentThreadId();
                dataProvider->removeDataProcessor(exportRecorder.data());
                dataProvider->pleaseStop();
            });

        //TODO: #vkutin implement filters.
        // exportRecorder->setExtraTranscodeParams(transcodeParams);

        connect(exportRecorder, &QnStreamRecorder::recordingProgress, q,
            &ExportMediaTool::valueChanged);

        connect(exportRecorder, &QnStreamRecorder::recordingFinished, q,
            [this]
            {
                qDebug() << "exportRecorder recordingFinished in thread" << QThread::currentThreadId();
                emit q->valueChanged(100);
            });

        if (settings.fileName.extension == FileExtension::avi)
            exportRecorder->setAudioCodec(AV_CODEC_ID_MP3); // transcode audio to MP3

        auto archiveReader = dynamic_cast<QnAbstractArchiveStreamReader*>(dataProvider.data());

        exportRecorder->clearUnprocessedData();
        exportRecorder->setEofDateTime(endTimeUs);
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

        state = ExportProcessStatus::exporting;
        emit q->statusChanged(state);
        return true;
    }

    void cancelExport()
    {
        NX_ASSERT(state == ExportProcessStatus::exporting);
        state = ExportProcessStatus::cancelling;
        emit q->statusChanged(state);

        // Minor optimization.
        dataProvider->removeDataProcessor(exportRecorder.data());
        //dataProvider->pleaseStop(); //< Will be stoped after export recorder stop.

        // Clean signature flag; in other case file will be recreated on writing finish.
        // TODO: #vasilenko get rid of this magic
        exportRecorder->setNeedCalcSignature(false);
        exportRecorder->pleaseStop();
    }

private:
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

        const auto tmpReader = settings.mediaResource->toResource()->createDataProvider(
            Qn::CR_Default);
        auto archiveReader = dynamic_cast<QnAbstractArchiveStreamReader*>(tmpReader);
        if (!archiveReader)
        {
            delete tmpReader;
            status = StreamRecorderError::invalidResourceType;
            return false;
        }

        archiveReader->setCycleMode(false);
        archiveReader->setQuality(MEDIA_Quality_ForceHigh, true);
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
        switch (state)
        {
            case ExportProcessStatus::exporting:
                state = (status == StreamRecorderError::noError)
                    ? ExportProcessStatus::success
                    : ExportProcessStatus::failure;
                break;
            case ExportProcessStatus::cancelling:
                state = ExportProcessStatus::cancelled;
                break;
            default:
                NX_ASSERT(false, "Should never get here");
                break;
        }
        emit q->statusChanged(state);

        if (state != ExportProcessStatus::success)
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

StreamRecorderError ExportMediaTool::status() const
{
    return d->status;
}

ExportProcessStatus ExportMediaTool::processStatus() const
{
    return d->state;
}

void ExportMediaTool::stop()
{
    d->cancelExport();
}

} // namespace desktop
} // namespace client
} // namespace nx
