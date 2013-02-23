#ifndef _VMAX480_READER_H__
#define _VMAX480_READER_H__

#include <QMutex>
#include <QWaitCondition>
#include <QDateTime>

#include "acs_stream_source.h"
#include "vmax480_helper.h"


class ACS_stream_source;
class TCPSocket;

class QnVMax480Provider
{
public:
    QnVMax480Provider(TCPSocket* socket);
    virtual ~QnVMax480Provider();

    void connect(const VMaxParamList& params, quint8 sequence, bool isLive);
    void disconnect();
    void archivePlay(const VMaxParamList& params);
    bool isConnected() const;
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
    bool m_connected;
    int m_spsPpsWidth;
    int m_spsPpsHeight;
    quint8 m_spsPpsBuffer[128];
    int m_spsPpsBufferLen;

    QMutex m_callbackMutex;
    QWaitCondition m_callbackCond;
    bool m_connectedInternal;
    QTime m_aliveTimer;
    TCPSocket* m_socket;
    quint8 m_reqSequence;
    quint8 m_curSequence;
    int m_channelNum;
};

#endif // _VMAX480_READER_H__
