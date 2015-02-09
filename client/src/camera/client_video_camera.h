#ifndef QN_CLIENT_VIDEO_CAMERA_H
#define QN_CLIENT_VIDEO_CAMERA_H

#include "cam_display.h"

#include <core/resource/resource_fwd.h>
#include <core/ptz/item_dewarping_params.h>

#include <recording/stream_recorder.h>

class QnlTimeSource;
class QnStatistics;
class QnResource;
class QnAbstractArchiveReader;
class QnTimePeriod;

class QnClientVideoCamera : public QObject {
    Q_OBJECT

    Q_ENUMS(ClientVideoCameraError)
public:
    enum ClientVideoCameraError {
        NoError = 0,
        FirstError = QnStreamRecorder::LastError,

        InvalidResourceType = FirstError,

        ErrorCount
    };

    static QString errorString(int errCode);

    QnClientVideoCamera(const QnMediaResourcePtr &resource, QnAbstractMediaStreamDataProvider* reader = 0);
    virtual ~QnClientVideoCamera();

    QnMediaResourcePtr resource();

    // this function must be called if stream was interupted or so; to synch audio and video again 
    //void streamJump(qint64 time);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    virtual QnResourcePtr getDevice() const;

    QnAbstractStreamDataProvider* getStreamreader();

    const QnStatistics* getStatistics(int channel = 0);
    QnCamDisplay* getCamDisplay();

    qint64 getCurrentTime() const;

    void setExternalTimeSource(QnlTimeSource* value) { m_extTimeSrc = value; }



    /*
    * Export motion stream to separate file
    */
    void setMotionIODevice(QSharedPointer<QBuffer>, int channel);
    QSharedPointer<QBuffer> motionIODevice(int channel);

    //TODO: #GDM Refactor parameter set to the structure
    void exportMediaPeriodToFile(const QnTimePeriod &timePeriod, 
                                 const QString& fileName,
                                 const QString& format, 
                                 const QnStorageResourcePtr &storage,
                                 QnStreamRecorder::Role role, 
                                 const QnImageFilterHelper &imageParameters,
                                 qint64 serverTimeZoneMs = Qn::InvalidUtcOffset);

    void setResource(QnMediaResourcePtr resource);
    QString exportedFileName() const;

    bool isDisplayStarted() const { return m_displayStarted; }
signals:
    void exportProgress(int progress);
    void exportFinished(int status, const QString &fileName);
    void exportStopped();

public slots:
    virtual void startDisplay();
    virtual void beforeStopDisplay();
    virtual void stopDisplay();

    void stopExport();
private:
    mutable QMutex m_exportMutex;
    QnMediaResourcePtr m_resource;
    QnCamDisplay m_camdispay;
    QnAbstractMediaStreamDataProvider* m_reader;

    QnlTimeSource* m_extTimeSrc;
    QnStreamRecorder* m_exportRecorder;
    QnAbstractArchiveReader* m_exportReader;
    QSharedPointer<QBuffer> m_motionFileList[CL_MAX_CHANNELS];
    bool m_displayStarted;
};

#endif //QN_CLIENT_VIDEO_CAMERA_H
