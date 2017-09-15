#ifndef QN_CLIENT_VIDEO_CAMERA_H
#define QN_CLIENT_VIDEO_CAMERA_H

#include "cam_display.h"

#include <core/resource/resource_fwd.h>
#include <core/ptz/item_dewarping_params.h>

#include <recording/stream_recorder.h>
#include <nx/streaming/config.h>

#include <utils/common/connective.h>

class QnlTimeSource;
class QnMediaStreamStatistics;
class QnResource;
class QnAbstractArchiveStreamReader;
class QnTimePeriod;

class QnClientVideoCamera : public Connective<QObject> {
    Q_OBJECT

    Q_ENUMS(ClientVideoCameraError)

    typedef Connective<QObject> base_type;
public:
    QnClientVideoCamera(const QnMediaResourcePtr &resource, QnAbstractMediaStreamDataProvider* reader = 0);
    virtual ~QnClientVideoCamera();

    QnMediaResourcePtr resource();

    // this function must be called if stream was interupted or so; to synch audio and video again
    //void streamJump(qint64 time);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    virtual QnResourcePtr getDevice() const;

    QnAbstractStreamDataProvider* getStreamreader();

    const QnMediaStreamStatistics* getStatistics(int channel = 0);
    QnCamDisplay* getCamDisplay();

    qint64 getCurrentTime() const;

    void setExternalTimeSource(QnlTimeSource* value) { m_extTimeSrc = value; }



    /*
    * Export motion stream to separate file
    */
    void setMotionIODevice(QSharedPointer<QBuffer>, int channel);
    QSharedPointer<QBuffer> motionIODevice(int channel);

    // TODO: #GDM Refactor parameter set to the structure
    void exportMediaPeriodToFile(const QnTimePeriod &timePeriod,
        const QString& fileName, const QString& format,
        QnStorageResourcePtr storage,
        StreamRecorderRole role,
        qint64 serverTimeZoneMs,
        qint64 timelapseFrameStepMs, /* Default value is 0 (timelapse disabled) */
        const QnLegacyTranscodingSettings& transcodeParams);

    void setResource(QnMediaResourcePtr resource);
    QString exportedFileName() const;

    bool isDisplayStarted() const { return m_displayStarted; }
signals:
    void exportProgress(int progress);
    void exportFinished(StreamRecorderErrorStruct reason, const QString &fileName);
    void exportStopped();

public slots:
    virtual void startDisplay();
    virtual void beforeStopDisplay();
    virtual void stopDisplay();

    void stopExport();
private:
    mutable QnMutex m_exportMutex;
    QnMediaResourcePtr m_resource;
    QnCamDisplay m_camdispay;   // TODO: #GDM refactor to scoped pointer
    QPointer<QnAbstractMediaStreamDataProvider> m_reader;   // TODO: #GDM refactor to unique pointer or 'owner' template

    QnlTimeSource* m_extTimeSrc;    // TODO: #GDM refactor to weak pointer
    QPointer<QnStreamRecorder> m_exportRecorder;
    QPointer<QnAbstractMediaStreamDataProvider> m_exportReader;
    QSharedPointer<QBuffer> m_motionFileList[CL_MAX_CHANNELS];
    bool m_displayStarted;
};

#endif //QN_CLIENT_VIDEO_CAMERA_H
