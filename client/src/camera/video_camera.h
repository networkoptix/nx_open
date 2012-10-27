#ifndef QN_VIDEO_CAMERA_H
#define QN_VIDEO_CAMERA_H

#include "cam_display.h"
#include "recording/stream_recorder.h"
#include "decoders/video/abstractdecoder.h"
#include "core/resource/media_resource.h"
#include "core/dataprovider/statistics.h"
#include "utils/media/externaltimesource.h"

class QnResource;
class QnStreamRecorder;
class QnAbstractArchiveReader;

class QnVideoCamera : public QObject {
    Q_OBJECT
public:
    QnVideoCamera(QnMediaResourcePtr resource, QnAbstractMediaStreamDataProvider* reader = 0);
    virtual ~QnVideoCamera();

    QnMediaResourcePtr resource();

    // this function must be called if stream was interupted or so; to synch audio and video again 
    //void streamJump(qint64 time);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    virtual QnResourcePtr getDevice() const;

    QnAbstractStreamDataProvider* getStreamreader();

    const QnStatistics* getStatistics(int channel = 0);
    QnCamDisplay* getCamDisplay();

    void setQuality(QnStreamQuality q, bool increase);
    qint64 getCurrentTime() const;

    void setExternalTimeSource(QnlTimeSource* value) { m_extTimeSrc = value; }

    // TODO: remove these
    bool isVisible() const { return m_isVisible; }
    void setVisible(bool value) { m_isVisible = value; }

    /*
    * Export motion stream to separate file
    */
    void setMotionIODevice(QSharedPointer<QBuffer>, int channel);

    void exportMediaPeriodToFile(qint64 startTime, qint64 endTime, const QString& fileName, const QString& format, QnStorageResourcePtr storage = QnStorageResourcePtr(), QnStreamRecorder::Role role = QnStreamRecorder::Role_FileExport);

    void setResource(QnMediaResourcePtr resource);
    void setExportProgressOffset(int value);
    int getExportProgressOffset() const;
    QString exportedFileName() const;
signals:
    void recordingFailed(QString errMessage);
    void exportProgress(int progress);
    void exportFailed(const QString &errMessage);
    void exportFinished(const QString &fileName);

public slots:
    virtual void startDisplay();
    virtual void beforeStopDisplay();
    virtual void stopDisplay();

    void stopExport();
    void onExportFinished(QString fileName);
    void onExportFailed(QString fileName);

protected slots:
    void at_exportProgress(int value);

private:
    mutable QMutex m_exportMutex;
    QnMediaResourcePtr m_resource;
    QnCamDisplay m_camdispay;
    QnAbstractMediaStreamDataProvider* m_reader;

    QnlTimeSource* m_extTimeSrc;
    bool m_isVisible;
    QnStreamRecorder* m_exportRecorder;
    QnAbstractArchiveReader* m_exportReader;
    int m_progressOffset;
    QSharedPointer<QBuffer> m_motionFileList[CL_MAX_CHANNELS];
};

#endif //QN_VIDEO_CAMERA_H
