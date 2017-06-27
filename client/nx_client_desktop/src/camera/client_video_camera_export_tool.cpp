#include "client_video_camera_export_tool.h"

#include <core/resource/media_resource.h>

#include <QtCore/QFileInfo>

#include <camera/client_video_camera.h>

QnClientVideoCameraExportTool::QnClientVideoCameraExportTool(
        const QnMediaResourcePtr &mediaResource,
        const QnTimePeriod &timePeriod,
        const QString &fileName,
        const QnImageFilterHelper &imageParameters,
        qint64 serverTimeZoneMs,
        qint64 timelapseFrameStepMs,
        QObject *parent)

    : base_type(parent)
    , m_camera(new QnClientVideoCamera(mediaResource))
    , m_timePeriod(timePeriod)
    , m_fileName(fileName)
    , m_parameters(imageParameters)
    , m_serverTimeZoneMs(serverTimeZoneMs)
    , m_timelapseFrameStepMs(timelapseFrameStepMs)
    , m_status(StreamRecorderError::noError)
{
    connect(m_camera,     &QnClientVideoCamera::exportProgress,   this,   &QnClientVideoCameraExportTool::valueChanged);
    connect(m_camera,     &QnClientVideoCamera::exportFinished,   this,   &QnClientVideoCameraExportTool::at_camera_exportFinished);
    connect(m_camera,     &QnClientVideoCamera::exportStopped,    this,   &QnClientVideoCameraExportTool::at_camera_exportStopped);
}

QnClientVideoCameraExportTool::~QnClientVideoCameraExportTool()
{}


void QnClientVideoCameraExportTool::start() {
    emit rangeChanged(0, 100);
    emit valueChanged(0);

    m_camera->exportMediaPeriodToFile(
                m_timePeriod,
                m_fileName,
                QFileInfo(m_fileName).suffix(),
                QnStorageResourcePtr(),
                StreamRecorderRole::fileExport,
                m_serverTimeZoneMs,
                m_timelapseFrameStepMs,
                m_parameters
                );
}

StreamRecorderError QnClientVideoCameraExportTool::status() const {
    return m_status;
}

void QnClientVideoCameraExportTool::stop() {
    m_camera->stopExport();
}

void QnClientVideoCameraExportTool::at_camera_exportFinished(const StreamRecorderErrorStruct& status,
    const QString& filename)
{
    Q_UNUSED(filename)
    m_status = status.lastError;
    finishExport(status.lastError == StreamRecorderError::noError);
}

void QnClientVideoCameraExportTool::at_camera_exportStopped() {
    finishExport(false);
}

void QnClientVideoCameraExportTool::finishExport(bool success) {
    if (!success)
        QFile::remove(m_fileName);

    emit finished(success, m_fileName);
}
