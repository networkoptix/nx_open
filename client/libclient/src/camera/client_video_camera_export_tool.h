#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <recording/time_period.h>
#include <recording/stream_recorder.h>
#include <transcoding/filters/filter_helper.h>

#include <utils/common/connective.h>

class QnClientVideoCamera;

class QnClientVideoCameraExportTool : public Connective<QObject>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnClientVideoCameraExportTool(
            const QnMediaResourcePtr &mediaResource,
            const QnTimePeriod &timePeriod,
            const QString &fileName,
            const QnImageFilterHelper &imageParameters,
            qint64 serverTimeZoneMs,
            qint64 timelapseFrameStepMs = 0, /* 0 means disabled timelapse */
            QObject *parent = 0);
    virtual ~QnClientVideoCameraExportTool();


    /**
     * @brief start                             Start exporting.
     */
    void start();

    /**
     * @brief status                            Camera exporting status.
     */
    StreamRecorderError status() const;

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
    void at_camera_exportFinished(const StreamRecorderErrorStruct& status,
        const QString& filename);

    void at_camera_exportStopped();

private:
    void finishExport(bool success);

private:
    QScopedPointer<QnClientVideoCamera> m_camera;
    QnTimePeriod m_timePeriod;
    QString m_fileName;
    QnImageFilterHelper m_parameters;
    qint64 m_serverTimeZoneMs;
    qint64 m_timelapseFrameStepMs;
    StreamRecorderError m_status;
};
