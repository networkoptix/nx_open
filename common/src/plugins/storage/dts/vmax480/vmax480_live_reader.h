#ifndef vmax480_live_h_1740
#define vmax480_live_h_1740

#include "core/dataprovider/spush_media_stream_provider.h"
#include "core/resource/resource_fwd.h"
#include "acs_stream_source.h"

class ACS_stream_source;

class QnVMax480LiveProvider: public CLServerPushStreamreader
{
public:
    QnVMax480LiveProvider(QnResourcePtr dev );
    virtual ~QnVMax480LiveProvider();
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;
private:
    static void receiveAudioStramCallback(PS_ACS_AUDIO_STREAM _stream, long long _user);
    static void receiveVideoStramCallback(PS_ACS_VIDEO_STREAM _stream, long long _user);
    static void receiveResultCallback(PS_ACS_RESULT _result, long long _user);

    void receiveVideoStream(PS_ACS_VIDEO_STREAM _stream);
    void receiveResult(PS_ACS_RESULT _result);
    void receiveAudioStream(PS_ACS_AUDIO_STREAM _stream);
    QDateTime fromNativeTimestamp(int mDate, int mTime, int mMillisec);
private:
    ACS_stream_source *m_ACSStream;
    QnNetworkResourcePtr m_networkRes;
    bool m_connected;
    int m_spsPpsWidth;
    int m_spsPpsHeight;
    quint8 m_spsPpsBuffer[128];
    int m_spsPpsBufferLen;

    QMutex m_callbackMutex;
    QWaitCondition m_callbackCond;
    bool m_connectedInternal;
    CLDataQueue m_internalQueue;
};

#endif //vmax480_live_h_1740
