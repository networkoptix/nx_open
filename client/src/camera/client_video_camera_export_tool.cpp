#include "client_video_camera_export_tool.h"

#include <QtCore/QFileInfo>

QnClientVideoCameraExportTool::QnClientVideoCameraExportTool(
        QnClientVideoCamera *camera,
        const QnTimePeriod &timePeriod,
        const QString &fileName,
        Qn::Corner timestamps,
        qint64 timeOffsetMs, qint64 serverTimeZoneMs,
        QRectF sourceRect,
        const ImageCorrectionParams &imageCorrectionParams,
        const QnItemDewarpingParams &itemDewarpingParams,
        int rotationAngle,
        qreal customAR,
        QObject *parent) :
    QObject(parent),
    m_camera(camera),
    m_timePeriod(timePeriod),
    m_fileName(fileName),
    m_timestampPos(timestamps),
    m_timeOffsetMs(timeOffsetMs),
    m_serverTimeZoneMs(serverTimeZoneMs),
    m_sourceRect(sourceRect),
    m_imageCorrectionParams(imageCorrectionParams),
    m_itemDewarpingParams(itemDewarpingParams),
    m_status(QnClientVideoCamera::NoError),
    m_rotationAngle(rotationAngle),
    m_customAR(customAR)
{
    connect(camera,     &QnClientVideoCamera::exportProgress,   this,   &QnClientVideoCameraExportTool::valueChanged);
    connect(camera,     &QnClientVideoCamera::exportFinished,   this,   &QnClientVideoCameraExportTool::at_camera_exportFinished);
    connect(camera,     &QnClientVideoCamera::exportStopped,    this,   &QnClientVideoCameraExportTool::at_camera_exportStopped);
}

void QnClientVideoCameraExportTool::start() {
    emit rangeChanged(0, 100);
    emit valueChanged(0);

    m_camera->exportMediaPeriodToFile(
                m_timePeriod.startTimeMs * 1000ll, (m_timePeriod.startTimeMs + m_timePeriod.durationMs) * 1000ll,
                m_fileName, QFileInfo(m_fileName).suffix(),
                QnStorageResourcePtr(),
                QnStreamRecorder::Role_FileExport,
                m_timestampPos,
                m_timeOffsetMs,
                m_serverTimeZoneMs,
                m_sourceRect,
                m_imageCorrectionParams,
                m_itemDewarpingParams,
                m_rotationAngle,
                m_customAR);
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
