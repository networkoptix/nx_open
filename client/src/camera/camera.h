#ifndef clcamera_h_1451
#define clcamera_h_1451

#include "camdisplay.h"
#include "videodisplay/complicated_item.h"
#include "recording/stream_recorder.h"
#include "decoders/video/abstractdecoder.h"
#include "core/resource/media_resource.h"
#include "core/dataprovider/streamdata_statistics.h"
#include "utils/media/externaltimesource.h"



class QnResource;
class CLVideoWindowItem;
class CLAbstractSceneItem;


class CLVideoCamera : public QObject, public CLAbstractComplicatedItem, public QnlTimeSource
{
    Q_OBJECT
public:
    // number of videovindows in array must be the same as device->getNumberOfVideoChannels
    CLVideoCamera(QnMediaResourcePtr device, CLVideoWindowItem* videovindow, bool generateEndOfStreamSignal, QnAbstractMediaStreamDataProvider* reader);
    virtual ~CLVideoCamera();

    virtual void startDispay();
    virtual void stopDispay();

    void startRecording();
    void stopRecording();
    bool isRecording();

    virtual void beforestopDispay();

    // this function must be called if stream was interupted or so; to synch audio and video again 
    //void streamJump(qint64 time);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    virtual QnResourcePtr getDevice() const;

    QnAbstractStreamDataProvider* getStreamreader();
    CLVideoWindowItem* getVideoWindow();
    const CLVideoWindowItem* getVideoWindow() const;

    virtual CLAbstractSceneItem* getSceneItem() const;

    const QnStatistics* getStatistics(int channel = 0);
    CLCamDisplay* getCamCamDisplay();

    void setQuality(QnStreamQuality q, bool increase);
    qint64 getCurrentTime() const;

    void setExternalTimeSource(QnlTimeSource* value) { m_extTimeSrc = value; }

signals:
    void reachedTheEnd();
    void recordingFailed(QString errMessage);

protected slots:
    void onReachedTheEnd();

private:
    QnMediaResourcePtr m_device;
    CLVideoWindowItem* m_videovindow;
    CLCamDisplay m_camdispay;
    QnStreamRecorder* m_recorder;

    QnAbstractMediaStreamDataProvider* m_reader;

    //QnStatistics* m_stat;
    bool mGenerateEndOfStreamSignal;
    QnlTimeSource* m_extTimeSrc;

};

#endif //clcamera_h_1451
