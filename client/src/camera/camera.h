#ifndef clcamera_h_1451
#define clcamera_h_1451

#include "camdisplay.h"
#include "recording/stream_recorder.h"
#include "decoders/video/abstractdecoder.h"
#include "core/resource/media_resource.h"
#include "core/dataprovider/statistics.h"
#include "utils/media/externaltimesource.h"

class QnResource;
class QnStreamRecorder;
class QnAbstractArchiveReader;

class CLVideoCamera : public QObject 
{
    Q_OBJECT
public:
    CLVideoCamera(QnMediaResourcePtr resource, bool generateEndOfStreamSignal = false, QnAbstractMediaStreamDataProvider* reader = 0);
    virtual ~CLVideoCamera();

    void startRecording();
    void stopRecording();
    bool isRecording();

    QnMediaResourcePtr resource();

    // this function must be called if stream was interupted or so; to synch audio and video again 
    //void streamJump(qint64 time);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    virtual QnResourcePtr getDevice() const;

    QnAbstractStreamDataProvider* getStreamreader();

    const QnStatistics* getStatistics(int channel = 0);
    CLCamDisplay* getCamDisplay();

    void setQuality(QnStreamQuality q, bool increase);
    qint64 getCurrentTime() const;

    void setExternalTimeSource(QnlTimeSource* value) { m_extTimeSrc = value; }

    // TODO: remove these
    bool isVisible() const { return m_isVisible; }
    void setVisible(bool value) { m_isVisible = value; }

    void exportMediaPeriodToFile(qint64 startTime, qint64 endTime, const QString& fileName, const QString& format, QnStorageResourcePtr storage = QnStorageResourcePtr());

    void setResource(QnMediaResourcePtr resource);
    void setExportProgressOffset(int value);
    int getExportProgressOffset() const;
signals:
    void reachedTheEnd();
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
    void onReachedTheEnd();
    void at_exportProgress(int value);
private:
    bool m_isVisible;
    QnMediaResourcePtr m_resource;
    CLCamDisplay m_camdispay;
    QnStreamRecorder* m_recorder;

    QnAbstractMediaStreamDataProvider* m_reader;

    //QnStatistics* m_stat;
    bool m_GenerateEndOfStreamSignal;
    QnlTimeSource* m_extTimeSrc;

    QnStreamRecorder* m_exportRecorder;
    QnAbstractArchiveReader* m_exportReader;
    int m_progressOffset;
};

#endif //clcamera_h_1451
