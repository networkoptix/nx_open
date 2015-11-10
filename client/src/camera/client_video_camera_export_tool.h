#ifndef CLIENT_VIDEO_CAMERA_EXPORT_TOOL_H
#define CLIENT_VIDEO_CAMERA_EXPORT_TOOL_H

#include <QtCore/QObject>

#include <recording/time_period.h>
#include <transcoding/filters/filter_helper.h>

class QnClientVideoCamera;

class QnClientVideoCameraExportTool : public QObject
{
    Q_OBJECT
public:
    QnClientVideoCameraExportTool(
            QnClientVideoCamera *camera,
            const QnTimePeriod &timePeriod,
            const QString &fileName,
            const QnImageFilterHelper &imageParameters,
            qint64 serverTimeZoneMs,
            QObject *parent = 0);

    /**
     * @brief start                             Start exporting.
     */
    void start();

    /**
     * @brief status                            Camera exporting status.
     */
    int status() const;

public slots:
    /**
     * @brief stop                              Stop exporting.
     */
    void stop();

signals:
    /**
     * @brief rangeChanged                      This signal is emitted when progress range of the process is changed.
     * @param from                              Minimum progress value.
     * @param to                                Maximum progress value.
     */
    void rangeChanged(int from, int to);

    /**
     * @brief valueChanged                      This signal is emitted when progress value is changed.
     * @param value                             Current progress value.
     */
    void valueChanged(int value);

    /**
     * @brief finished                          This signal is emitted when the process is finished.
     * @param success                           True if the process is finished successfully, false otherwise.
     */
    void finished(bool success, const QString &filename);

private slots:
    void at_camera_exportFinished(int status, const QString &filename);
    void at_camera_exportStopped();

private:
    void finishExport(bool success);

private:
    QnClientVideoCamera *m_camera;
    QnTimePeriod m_timePeriod;
    QString m_fileName;
    QnImageFilterHelper m_parameters;
    qint64 m_serverTimeZoneMs;
    int m_status;
};

#endif // CLIENT_VIDEO_CAMERA_EXPORT_TOOL_H
