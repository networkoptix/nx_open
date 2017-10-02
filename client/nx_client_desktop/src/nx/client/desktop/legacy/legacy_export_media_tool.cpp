#include "legacy_export_media_tool.h"

#include <QtCore/QFileInfo>

#include <core/resource/media_resource.h>
#include <camera/client_video_camera.h>

#include <nx/client/desktop/legacy/legacy_export_settings.h>

namespace nx {
namespace client {
namespace desktop {
namespace legacy {

struct LegacyExportMediaTool::Private
{
    LegacyExportMediaSettings settings;
    QScopedPointer<QnClientVideoCamera> camera;
    StreamRecorderError status = StreamRecorderError::noError;

    Private(const LegacyExportMediaSettings& settings):
        settings(settings),
        camera(new QnClientVideoCamera(settings.mediaResource))
    {
    }
};

LegacyExportMediaTool::LegacyExportMediaTool(
    const LegacyExportMediaSettings& settings,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(settings))
{
    connect(d->camera, &QnClientVideoCamera::exportProgress, this,
        &LegacyExportMediaTool::valueChanged);
    connect(d->camera, &QnClientVideoCamera::exportFinished, this,
        &LegacyExportMediaTool::at_camera_exportFinished);
    connect(d->camera, &QnClientVideoCamera::exportStopped, this,
        &LegacyExportMediaTool::at_camera_exportStopped);
}

LegacyExportMediaTool::~LegacyExportMediaTool()
{
}


void LegacyExportMediaTool::start()
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

StreamRecorderError LegacyExportMediaTool::status() const
{
    return d->status;
}

void LegacyExportMediaTool::stop()
{
    d->camera->stopExport();
}

void LegacyExportMediaTool::at_camera_exportFinished(
    const StreamRecorderErrorStruct& status,
    const QString& /*filename*/)
{
    d->status = status.lastError;
    finishExport(status.lastError == StreamRecorderError::noError);
}

void LegacyExportMediaTool::at_camera_exportStopped()
{
    finishExport(false);
}

void LegacyExportMediaTool::finishExport(bool success)
{
    if (!success)
        QFile::remove(d->settings.fileName);

    emit finished(success, d->settings.fileName);
}

} // namespace legacy
} // namespace desktop
} // namespace client
} // namespace nx
