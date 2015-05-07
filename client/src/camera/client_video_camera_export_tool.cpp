#include "client_video_camera_export_tool.h"

#include <QtCore/QFileInfo>

#include <camera/client_video_camera.h>

QnClientVideoCameraExportTool::QnClientVideoCameraExportTool(
        QnClientVideoCamera *camera,
        const QnTimePeriod &timePeriod,
        const QString &fileName,
        const QnImageFilterHelper &imageParameters,
        qint64 serverTimeZoneMs,
        QObject *parent) :
    QObject(parent),
    m_camera(camera),
    m_timePeriod(timePeriod),
    m_fileName(fileName),
    m_parameters(imageParameters),
    m_serverTimeZoneMs(serverTimeZoneMs),
    m_status(QnClientVideoCamera::NoError)
{
    connect(camera,     &QnClientVideoCamera::exportProgress,   this,   &QnClientVideoCameraExportTool::valueChanged);
    connect(camera,     &QnClientVideoCamera::exportFinished,   this,   &QnClientVideoCameraExportTool::at_camera_exportFinished);
    connect(camera,     &QnClientVideoCamera::exportStopped,    this,   &QnClientVideoCameraExportTool::at_camera_exportStopped);
}

void QnClientVideoCameraExportTool::start() {
    emit rangeChanged(0, 100);
    emit valueChanged(0);

    m_camera->exportMediaPeriodToFile(
                m_timePeriod,
                m_fileName,
                QFileInfo(m_fileName).suffix(),
                QnStorageResourcePtr(),
                QnStreamRecorder::Role_FileExport,
                m_parameters);
}

int QnClientVideoCameraExportTool::status() const {
    return m_status;
}

void QnClientVideoCameraExportTool::stop() {
    m_camera->stopExport();
}

void QnClientVideoCameraExportTool::at_camera_exportFinished(int status, const QString &filename) {
    Q_UNUSED(filename)
    m_status = status;
    finishExport(status == QnClientVideoCamera::NoError);
}

void QnClientVideoCameraExportTool::at_camera_exportStopped() {
    m_camera->deleteLater();
    finishExport(false);
}

void QnClientVideoCameraExportTool::finishExport(bool success) {
    if (!success)
        QFile::remove(m_fileName);

    emit finished(success, m_fileName);
}
