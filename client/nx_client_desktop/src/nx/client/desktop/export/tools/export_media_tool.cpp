#include "export_media_tool.h"

#include <QtCore/QFileInfo>

#include <core/resource/media_resource.h>
#include <camera/client_video_camera.h>

#include <nx/client/desktop/export/data/export_media_settings.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportMediaTool::Private
{
    ExportMediaSettings settings;
    QScopedPointer<QnClientVideoCamera> camera;
    StreamRecorderError status = StreamRecorderError::noError;

    Private(const ExportMediaSettings& settings):
        settings(settings),
        camera(new QnClientVideoCamera(settings.mediaResource))
    {
    }
};

ExportMediaTool::ExportMediaTool(
    const ExportMediaSettings& settings,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(settings))
{
    connect(d->camera, &QnClientVideoCamera::exportProgress, this,
        &ExportMediaTool::valueChanged);
    connect(d->camera, &QnClientVideoCamera::exportFinished, this,
        &ExportMediaTool::at_camera_exportFinished);
    connect(d->camera, &QnClientVideoCamera::exportStopped, this,
        &ExportMediaTool::at_camera_exportStopped);
}

ExportMediaTool::~ExportMediaTool()
{
}


void ExportMediaTool::start()
{
    emit rangeChanged(0, 100);
    emit valueChanged(0);

    d->camera->exportMediaPeriodToFile(
        d->settings.timePeriod,
        d->settings.fileName,
        QFileInfo(d->settings.fileName).suffix(),
        QnStorageResourcePtr(),
        StreamRecorderRole::fileExport,
        d->settings.serverTimeZoneMs,
        d->settings.timelapseFrameStepMs,
        d->settings.imageParameters
    );
}

StreamRecorderError ExportMediaTool::status() const
{
    return d->status;
}

void ExportMediaTool::stop()
{
    d->camera->stopExport();
}

void ExportMediaTool::at_camera_exportFinished(
    const StreamRecorderErrorStruct& status,
    const QString& /*filename*/)
{
    d->status = status.lastError;
    finishExport(status.lastError == StreamRecorderError::noError);
}

void ExportMediaTool::at_camera_exportStopped()
{
    finishExport(false);
}

void ExportMediaTool::finishExport(bool success)
{
    if (!success)
        QFile::remove(d->settings.fileName);

    emit finished(success, d->settings.fileName);
}

} // namespace desktop
} // namespace client
} // namespace nx
