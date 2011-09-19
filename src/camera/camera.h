#ifndef clcamera_h_1451
#define clcamera_h_1451

#include "camdisplay.h"
#include "../statistics/statistics.h"
#include "../streamreader/streamreader.h"
#include "videodisplay/complicated_item.h"
#include "stream_recorder.h"
#include "decoders/video/abstractdecoder.h"

class CLDevice;
class CLVideoWindowItem;
class CLAbstractSceneItem;

class CLVideoCamera : public QObject, public CLAbstractComplicatedItem
{
    Q_OBJECT
public:
    // number of videovindows in array must be the same as device->getNumberOfVideoChannels
    CLVideoCamera(CLDevice* device, CLVideoWindowItem* videovindow, bool generateEndOfStreamSignal, CLStreamreader* reader);
    virtual ~CLVideoCamera();

    virtual void startDispay();
    virtual void stopDispay();

    void startRecording();
    void stopRecording();
    bool isRecording();

    virtual void beforestopDispay();

    // this function must be called if stream was interupted or so; to synch audio and video again 
    void streamJump(qint64 time);

    void setLightCPUMode(CLAbstractVideoDecoder::DecodeMode val);

    virtual CLDevice* getDevice() const;

    CLStreamreader* getStreamreader();
    CLVideoWindowItem* getVideoWindow();
    const CLVideoWindowItem* getVideoWindow() const;

    virtual CLAbstractSceneItem* getSceneItem() const;

    CLStatistics* getStatistics();
    CLCamDisplay* getCamCamDisplay();

    void setQuality(CLStreamreader::StreamQuality q, bool increase);
    quint64 currentTime() const;

signals:
    void reachedTheEnd();

protected slots:
    void onReachedTheEnd();

private:
    CLDevice* m_device;
    CLVideoWindowItem* m_videovindow;
    CLCamDisplay m_camdispay;
    CLStreamRecorder* m_recorder;

    CLStreamreader* m_reader;

    CLStatistics* m_stat;
    bool mGenerateEndOfStreamSignal;

};

#endif //clcamera_h_1451
