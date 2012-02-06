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
    // number of videovindows in array must be the same as device->getNumberOfVideoChannels
    CLVideoCamera(QnMediaResourcePtr device, bool generateEndOfStreamSignal, QnAbstractMediaStreamDataProvider* reader);
    virtual ~CLVideoCamera();

    virtual void startDisplay();
    virtual void beforeStopDisplay();
    virtual void stopDisplay();

    void startRecording();
    void stopRecording();
    bool isRecording();


    // this function must be called if stream was interupted or so; to synch audio and video again 
    //void streamJump(qint64 time);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    virtual QnResourcePtr getDevice() const;

    QnAbstractStreamDataProvider* getStreamreader();

    const QnStatistics* getStatistics(int channel = 0);
    CLCamDisplay* getCamCamDisplay();

    void setQuality(QnStreamQuality q, bool increase);
    qint64 getCurrentTime() const;

    void setExternalTimeSource(QnlTimeSource* value) { m_extTimeSrc = value; }

    bool isVisible() const { return m_isVisible; }
    void setVisible(bool value) { m_isVisible = value; }

    void exportMediaPeriodToFile(qint64 startTime, qint64 endTime, const QString& fileName);
signals:
    void reachedTheEnd();
    void recordingFailed(QString errMessage);
    void exportProgress(int progress);
    void exportFailed(const QString &errMessage);
    void exportFinished(const QString &fileName);
public slots:
    void stopExport();
    void onExportFinished();
protected slots:
    void onReachedTheEnd();
private:
    bool m_isVisible;
    QnMediaResourcePtr m_device;
    CLCamDisplay m_camdispay;
    QnStreamRecorder* m_recorder;

    QnAbstractMediaStreamDataProvider* m_reader;

    //QnStatistics* m_stat;
    bool m_GenerateEndOfStreamSignal;
    QnlTimeSource* m_extTimeSrc;

    QnStreamRecorder* m_exportRecorder;
    QnAbstractArchiveReader* m_exportReader;
};

#endif //clcamera_h_1451
