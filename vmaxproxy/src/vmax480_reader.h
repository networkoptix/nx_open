#ifndef _VMAX480_READER_H__
#define _VMAX480_READER_H__

#include <QQueue>
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
    void archivePlay(const VMaxParamList& params, quint8 sequence);
    void pointsPlay(const VMaxParamList& params, quint8 sequence);
    bool isConnected() const;
    void requestMonthInfo(const VMaxParamList& params, quint8 sequence);
    void requestDayInfo(const VMaxParamList& params, quint8 sequence);
    void requestRange(const VMaxParamList& params, quint8 sequence);

    void keepAlive();
    bool waitForConnected(int timeoutMs);

    void addChannel(const VMaxParamList& params);
    void removeChannel(const VMaxParamList& params);
private:
    static void receiveAudioStramCallback(PS_ACS_AUDIO_STREAM _stream, long long _user);
    static void receiveVideoStramCallback(PS_ACS_VIDEO_STREAM _stream, long long _user);
    static void receiveResultCallback(PS_ACS_RESULT _result, long long _user);

    void receiveVideoStream(PS_ACS_VIDEO_STREAM _stream);
    void receiveResult(PS_ACS_RESULT _result);
    void receiveAudioStream(PS_ACS_AUDIO_STREAM _stream);
    QDateTime fromNativeTimestamp(int mDate, int mTime, int mMillisec);
    void archivePlayInternal(const VMaxParamList& params, quint8 sequence);
    void playPointsInternal();
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
    //int m_channelNum;
    QQueue<int> m_monthRequests;
    QQueue<int> m_daysRequests;
    unsigned char recordedDayInfo[VMAX_MAX_CH][1440+60];
    bool m_archivePlayProcessing;
    VMaxParamList m_newPlayCommand;
    QQueue<qint64> m_playPoints;
    bool m_pointsPlayMode;
    int m_channelMask;

    enum PointsPlayState {PP_None, PP_WaitAnswer, PP_GotAnswer};
    PointsPlayState m_ppState;

    QMutex m_channelMutex;
    QWaitCondition m_channelCond;
    bool m_channelProcessed;
    bool m_needStop;
};

#endif // _VMAX480_READER_H__
