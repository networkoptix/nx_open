#include <QUrl>
#include <qDebug>
#include <QThread>
#include <QElapsedTimer>

#include "vmax480_reader.h"
#include "socket.h"
#include "acs_codec.h"
#include "nalconstructor.h"

class QnSleep : public QThread
{
public:

    static void msleep ( unsigned long msecs )
    {
        QThread::msleep(msecs);
    }
};

static const int CHANNEL_TIMEOUT = 1000 * 5;

int create_vmax_sps_pps(
                   int frameWidth,
                   int frameHeight,
                   unsigned char* data, int max_datalen)
{
    bs_t stream;
    bs_init(&stream,data,max_datalen); // just stream init

    bs_write_startcode(&stream); // write start code

    //int header = ( 0x00 << 7 ) | ( i_ref_idc << 5 ) | i_type;
    //for sps header = 103 (0x67)

    bs_write(&stream,8,0x67); // header

    //======= sps main======
    bs_write( &stream, 8, 66 ); //i_profile_idc = 66

    bs_write( &stream, 1, 0 );
    bs_write( &stream, 1, 0 );
    bs_write( &stream, 1, 0 );
    bs_write( &stream, 1, 0 );

    bs_write( &stream, 4, 0 );    /* reserved_zero_4bits */

    bs_write( &stream, 8, 30 ); //i_level_idc = 32

    bs_write_ue( &stream, 0 ); //seq_parameter_set_id

    bs_write_ue( &stream, 5 ); //log2_max_frame_num_minus4 - 4 ????
    bs_write_ue( &stream, 0 ); //pic_order_cnt_type
    //if( pic_order_cnt_type == 0 )
    {
        bs_write_ue( &stream, 6 ); //log2_max_pic_order_cnt_lsb_minus4
    }

    bs_write_ue( &stream, 1 ); // num_ref_frames
    bs_write( &stream, 1, 0); //gaps_in_frame_num_value_allowed_flag

    int i_mb_width = frameWidth/16; // in first version we belive that width is devided on 16
    int i_mb_height = frameHeight/16; // in first version we belive that width is devided on 16

    bs_write_ue( &stream, i_mb_width - 1 );
    bs_write_ue( &stream, i_mb_height - 1);
    bs_write( &stream, 1, 1); //frame_mbs_only_flag

    bs_write( &stream, 1, 1); // direct_8x8_inference_flag

    bs_write( &stream, 1, 0 ); //frame_cropping_flag; in first version = 0;

    bs_write( &stream, 1, 0 ); //vui_parameters_present_flag

    bs_rbsp_trailing( &stream );
    //======= sps main======

    bs_write_startcode(&stream); // write start code
    //int header = ( 0x00 << 7 ) | ( i_ref_idc << 5 ) | i_type;
    //for sps header = 104 (0x68)
    bs_write(&stream,8,0x68); // header
    //=======pps main========

    bs_write_ue( &stream, 0); //pic_parameter_set_id
    bs_write_ue( &stream, 0); //seq_parameter_set_id
    bs_write( &stream, 1, 0 ); //cabac

    bs_write( &stream, 1, 0 ); //pic_order_present_flag
    bs_write_ue( &stream, 0 ); //num_slice_groups_minus1

    bs_write_ue( &stream, 0 ); //num_ref_idx_l0_active_minus1
    bs_write_ue( &stream, 0 ); //num_ref_idx_l1_active_minus1
    bs_write( &stream, 1, 0 ); //weighted_pred_flag
    bs_write( &stream, 2, 0 ); //weighted_bipred_idc

    bs_write_se( &stream, 0 ); //pic_init_qp_minus26
    bs_write_se( &stream, 0 ); //pic_init_qs_minus26
    bs_write_se( &stream, 2 ); //chroma_qp_index_offset

    bs_write( &stream, 1, 0 ); // no deblocking filter

    bs_write( &stream, 1, 1); //constrained_intra_pred_flag
    bs_write( &stream, 1, 0 ); //redundant_pic_cnt_present_flag

    bs_rbsp_trailing( &stream );
    //=======================

    return stream.p - stream.p_start;

}

QDateTime QnVMax480Provider::fromNativeTimestamp(int mDate, int mTime, int mMillisec)
{
    int _year = mDate / 10000;
    int _month = (mDate % 10000) / 100;
    int _day = (mDate % 10000) % 100;

    int hour = mTime /10000;
    int min = (mTime%10000)/100;
    int sec = (mTime%10000)%100;
    int milli = (mMillisec);

    return QDateTime(QDate(_year, _month, _day), QTime(hour, min, sec, milli));
}

void toNativeTimestamp(QDateTime timestamp, quint32* date, quint32* time, quint32* ms)
{
    *date = timestamp.date().year() * 10000 + timestamp.date().month()  * 100 + timestamp.date().day();
    *time = timestamp.time().hour() * 10000 + timestamp.time().minute() * 100 + timestamp.time().second();
    if (ms)
        *ms = timestamp.time().msec();
}

// ----------------------------------- QnVMax480LiveProvider -----------------------

QnVMax480Provider::QnVMax480Provider(TCPSocket* socket):
    m_ACSStream(0),
    m_connected(false),
    m_spsPpsWidth(-1),
    m_spsPpsHeight(-1),
    m_spsPpsBufferLen(0),
    m_connectedInternal(false),
    m_socket(socket),
    m_channelMask(0),
    m_reqSequence(0),
    m_curSequence(0),
    m_archivePlayProcessing(false),
    m_pointsPlayMode(false),
    //m_pointsNeedFrame(false)
    m_ppState(PP_None),
    m_needStop(false)
{
}

QnVMax480Provider::~QnVMax480Provider()
{

}

bool QnVMax480Provider::waitForConnected(int timeoutMs)
{
    QMutexLocker lock(&m_callbackMutex);
    QTime t;
    t.restart();
    while (!m_connectedInternal && t.elapsed() < timeoutMs)
        m_callbackCond.wait(&m_callbackMutex, timeoutMs);
    return m_connectedInternal;
}

void QnVMax480Provider::connect(const VMaxParamList& params, quint8 sequence, bool isLive)
{
    if (m_connected)
        return;

    create_acs_source(&m_ACSStream);

    //m_ACSStream->setRecvAudioStreamCallback(receiveAudioStramCallback , (long long)this);
    m_ACSStream->setRecvVideoStreamCallback(receiveVideoStramCallback , (long long)this);
    m_ACSStream->setRecvResultCallback(receiveResultCallback , (long long)this);

    QUrl url(params.value("url"));

    /*
    m_channelNum = url.queryItemValue("channel").toInt();
    if (m_channelNum > 0)
        m_channelNum--;
    */
    //m_channelNum = params.value("channel").toInt();
    m_channelMask = params.value("channel").toInt();

    S_CONNECT_PARAM connectParam;
    connectParam.mUrl = url.host().toStdWString();
    connectParam.mTcpPort = url.port(80);
    connectParam.mUserID = QString::fromUtf8(params.value("login")).toStdWString();
    connectParam.mUserPwd = QString::fromUtf8(params.value("password")).toStdWString();

    connectParam.mIsLive = isLive;

    //call connect function
    m_reqSequence = m_curSequence = sequence;
    m_ACSStream->connect(&connectParam);

    m_connected = true;

    //m_ACSStream->checkAlive();
    //m_ACSStream->requestRecordDateTime();
    m_aliveTimer.restart();
}

void QnVMax480Provider::keepAlive()
{
    if (isConnected() && m_connectedInternal)
        m_ACSStream->checkAlive();
}

void QnVMax480Provider::disconnect()
{
    if (!m_connected)
        return;

    QMutexLocker lock(&m_callbackMutex);
    m_needStop = true;
    QElapsedTimer t;
    t.restart();
    while (m_archivePlayProcessing && t.elapsed() < 1000 * 5)
    {
        qDebug() << "wait for play request finished...";
        m_callbackCond.wait(&m_callbackMutex, 200);
    }

    m_ACSStream->disconnect();

    if (m_connectedInternal) {
        m_callbackCond.wait(&m_callbackMutex, 200);
        m_connectedInternal = false;
    }

    if(m_ACSStream) {
        m_ACSStream->close();
        destroy_acs_source(&m_ACSStream);
        m_ACSStream = 0;
        m_connected = false;
    }
}

void QnVMax480Provider::archivePlay(const VMaxParamList& params, quint8 sequence)
{
    if (!m_ACSStream)
        return;

    QMutexLocker lock(&m_callbackMutex);
    archivePlayInternal(params, sequence);
}

void QnVMax480Provider::archivePlayInternal(const VMaxParamList& params, quint8 sequence)
{
    m_pointsPlayMode = false;

    qint64 posUsec = params.value("pos").toLongLong();
    int speed = params.value("speed").toInt();
    quint32 startDate;
    quint32 startTime;
    toNativeTimestamp(QDateTime::fromMSecsSinceEpoch(posUsec/1000), &startDate, &startTime, 0);

    m_reqSequence = sequence;
    ACS_stream_source::PLAYMODE playMode;
    if (speed > 0)
        playMode = ACS_stream_source::FORWARDPLAY;
    else if (speed == 0)
        playMode = ACS_stream_source::STOP;
    else
        playMode = ACS_stream_source::BACKWARDPLAY;

    if (!m_archivePlayProcessing) {
        qDebug() << "Forward play. pos=" << fromNativeTimestamp(startDate, startTime, 0).toString("dd.MM.yyyy hh:mm.ss") << "speed=" << speed << "seq=" << sequence;
        m_archivePlayProcessing = true;
        m_ACSStream->requestPlayMode(playMode, speed > 1 ? 8 : 1, startDate, startTime, false);
    }
    else {
        m_newPlayCommand = params;
    }
}

void QnVMax480Provider::pointsPlay(const VMaxParamList& params, quint8 sequence)
{
    if (!m_ACSStream)
        return;

    QList<QByteArray> points = params.value("points").split(';'); // points at seconds
    m_playPoints.clear();

    QMutexLocker lock(&m_callbackMutex);

    m_pointsPlayMode = true;
    m_reqSequence = sequence;

    for (int i = 0; i < points.size(); ++i)
        m_playPoints << points[i].toLongLong() * 1000ll; // convert to ms

    qDebug() << "got points:";
    for (int i = 0; i < points.size(); ++i)
    {
        qDebug() << QDateTime::fromMSecsSinceEpoch(m_playPoints[i]).toString(QLatin1String("dd hh.mm.ss.zzz"));
    }

    if (!m_archivePlayProcessing)
        playPointsInternal();
}

void QnVMax480Provider::playPointsInternal()
{
    //m_pointsNeedFrame = true;
    if (!m_playPoints.isEmpty())
    {
        quint32 playDate, playTime;
        toNativeTimestamp(QDateTime::fromMSecsSinceEpoch(m_playPoints.dequeue()), &playDate, &playTime, 0);
        qDebug() << "go next thumbnails point";
        m_ppState = PP_WaitAnswer;
        m_ACSStream->requestPlayMode(ACS_stream_source::FORWARDPLAY, 1, playDate, playTime, true);
    }
    else {
        qDebug() << "stop thumbnails point";
        m_ACSStream->requestPlayMode(ACS_stream_source::STOP, 0, 0, 0, false);
        //m_pointsPlayMode = false;
        m_ppState = PP_None;
    }
}

void QnVMax480Provider::addChannel(const VMaxParamList& params)
{
    if (!m_ACSStream)
        return;

    int channel = params.value("channel").toInt();
    qDebug() << "m_ACSStream->openChannel=" << channel;

    QMutexLocker lock(&m_channelMutex);
    m_channelProcessed = false;
    m_ACSStream->openChannel(channel);
    m_channelMask |= channel;

    QTime t;
    t.restart();
    while (!m_channelProcessed && t.elapsed() < CHANNEL_TIMEOUT)
        m_channelCond.wait(&m_channelMutex, CHANNEL_TIMEOUT);
}

void QnVMax480Provider::removeChannel(const VMaxParamList& params)
{
    if (!m_ACSStream)
        return;

    int channel = params.value("channel").toInt();
    qDebug() << "m_ACSStream->closeChannel" << channel;

    QMutexLocker lock(&m_channelMutex);
    m_channelProcessed = false;
    m_ACSStream->closeChannel(channel);
    m_channelMask &= ~channel;

    QTime t;
    t.restart();
    while (!m_channelProcessed && t.elapsed() < CHANNEL_TIMEOUT)
        m_channelCond.wait(&m_channelMutex, CHANNEL_TIMEOUT);
}

void QnVMax480Provider::requestMonthInfo(const VMaxParamList& params, quint8 sequence)
{
    if (!m_ACSStream)
        return;

    int month = params.value("month").toInt();

    {
        QMutexLocker lock(&m_callbackMutex);
        m_monthRequests << month;
    }

    m_ACSStream->requestRecordMonth(month);
}

void QnVMax480Provider::requestDayInfo(const VMaxParamList& params, quint8 sequence)
{
    if (!m_ACSStream)
        return;

    int dayNum = params.value("day").toInt();

    {
        QMutexLocker lock(&m_callbackMutex);
        m_daysRequests << dayNum;
    }

    m_ACSStream->requestRecordDay(dayNum);
}

void QnVMax480Provider::requestRange(const VMaxParamList&, quint8)
{
    if (!m_ACSStream)
        return;

    m_ACSStream->requestRecordDateTime();
}

bool QnVMax480Provider::isConnected() const
{
    return m_connected;
}

void QnVMax480Provider::receiveAudioStramCallback(PS_ACS_AUDIO_STREAM _stream, long long _user)
{
    QnVMax480Provider* object = (QnVMax480Provider*)_user;
    object->receiveAudioStream(_stream);
}

void QnVMax480Provider::receiveVideoStramCallback(PS_ACS_VIDEO_STREAM _stream, long long _user)
{
    QnVMax480Provider* object = (QnVMax480Provider*)_user;
    object->receiveVideoStream(_stream);
}
void QnVMax480Provider::receiveResultCallback(PS_ACS_RESULT _result, long long _user)
{
    QnVMax480Provider* object = (QnVMax480Provider*)_user;
    object->receiveResult(_result);
}

void QnVMax480Provider::receiveVideoStream(S_ACS_VIDEO_STREAM* _stream)
{

    //qDebug() << "receiveVideoStream" << _stream->mCh;
    quint8 VMaxHeader[16];
    bool isIFrame = false;
    bool buildinSps = false;
    {
        QMutexLocker lock(&m_callbackMutex);

        if(!m_connected)
            return;

        if (m_reqSequence != m_curSequence) {
            qDebug() << "m_reqSequence != m_curSequence" << m_reqSequence << "!=" << m_curSequence;
            QnSleep::msleep(20);
            return;
        }

        if (m_pointsPlayMode)
        {
            if (m_ppState != PP_GotAnswer)
                return;

            qDebug() << "got thumbnails frame";
        }

        QDateTime packetTimestamp = fromNativeTimestamp(_stream->mDate, _stream->mTime, _stream->mMillisec);

        if (_stream->mWidth != m_spsPpsWidth || _stream->mHeight != m_spsPpsHeight) {
            m_spsPpsBufferLen = create_vmax_sps_pps(_stream->mWidth, _stream->mHeight, m_spsPpsBuffer, sizeof(m_spsPpsBuffer));
            m_spsPpsWidth = _stream->mWidth;
            m_spsPpsHeight = _stream->mHeight;
        }

        if (_stream->mSrcSize >= 5) {
            quint8 nalType = _stream->mSrc[4] & 0x1f;
            buildinSps =  nalType == 7;
            if (buildinSps)
                isIFrame = true;
            else
                isIFrame = nalType == 5;
        }

        int dataSize = _stream->mSrcSize + (isIFrame && !buildinSps ? m_spsPpsBufferLen : 0);

        VMaxHeader[0] = m_reqSequence;
        VMaxHeader[1] = VMAXDT_GotVideoPacket;
        VMaxHeader[2] = (quint8) _stream->mSrcType;
        VMaxHeader[3] = _stream->mCh << 4;
        if (isIFrame)
            VMaxHeader[3] |= 1;
        *(quint32*)(VMaxHeader+4) = dataSize;
        //*(quint64*)(VMaxHeader+8) = QDateTime::currentDateTime().toMSecsSinceEpoch()*1000;
        *(quint64*)(VMaxHeader+8) = packetTimestamp.toMSecsSinceEpoch() * 1000;

        if (m_pointsPlayMode)
            playPointsInternal();
    }

    m_socket->send(VMaxHeader, sizeof(VMaxHeader));
    if (isIFrame && !buildinSps)
        m_socket->send(m_spsPpsBuffer, m_spsPpsBufferLen);
    m_socket->send(_stream->mSrc, _stream->mSrcSize);

}

QString codeToString(int code)
{
    switch(code)
    {
    case RESULT_CONNECT:
            return "RESULT_CONNECT";
    case RESULT_DISCONNECTED:
            return "RESULT_DISCONNECTED";
    case RESULT_LOGIN:
        return "RESULT_LOGIN";
    case RESULT_ERROR:
        return "RESULT_ERROR";
    case RESULT_ALIVECHK:
        return "RESULT_ALIVECHK";
    case RESULT_SEARCH_PLAY:
        return "RESULT_SEARCH_PLAY";
    case RESULT_REC_DATE_TIME:
        return "RESULT_REC_DATE_TIME";
    case  RESULT_REC_INFO_MONTH:
        return "RESULT_REC_INFO_MONTH";
    case  RESULT_REC_TIME_TABLE:
        return "RESULT_REC_TIME_TABLE";
    case RESULT_OPEN_VIDEO:
        return "RESULT_OPEN_VIDEO";
    case RESULT_OPEN_AUDIO:
        return "RESULT_OPEN_AUDIO";
    case RESULT_CLOSE_VIDEO:
        return "RESULT_CLOSE_VIDEO";
    case RESULT_CLOSE_AUDIO:
        return "RESULT_CLOSE_AUDIO";
    }
    return "Unknown";
}

void QnVMax480Provider::receiveResult(S_ACS_RESULT* _result)
{
    QMutexLocker lock(&m_callbackMutex);

    qDebug() << "got result callback. code=" << codeToString(_result->mType);

    int openChannel = 0;
    int recordInfoMonth;

    switch(_result->mType)
    {
    case RESULT_CONNECT:
        //m_connectedInternal = true;
        break;
    case RESULT_DISCONNECTED:
        m_connectedInternal = false;
        m_callbackCond.wakeOne();
    case RESULT_LOGIN:
        {
            if( _result->mResult == true )
            {
                //m_ACSStream->requestRecordDateTime();

                if (m_channelMask > 0) {
                    qDebug() << "open channel onStartup=" << m_channelMask;
                    m_ACSStream->openChannel(m_channelMask);
                    //m_ACSStream->openAudioChannel(m_channelMask);
                }
                else {
                    m_connectedInternal = true;
                    m_callbackCond.wakeOne();
                }

                //m_connectedInternal = true;
                //m_callbackCond.wakeOne();
            }
            break;
        }

    case RESULT_ERROR:
        {
            if( _result->mData1 != NULL )
            {
                PS_ERROR_PARAM error = (PS_ERROR_PARAM)(_result->mData1);

                if( error->mErrorCode == 0xE0000001 )
                    ; //OutputDebugString(_T("Unregister user\n"));
                if( error->mErrorCode == 0xE0000002 )
                    ; //OutputDebugString(_T("Incorrect password\n"));
                if( error->mErrorCode == 0xE0000003 )
                    ;
                if( error->mErrorCode == 0xE0000004 )
                    ; //OutputDebugString(_T("Exceeded visitor\n"));
                if( error->mErrorCode == 0xEFFFFFFF )
                {
                    m_connectedInternal = false;
                    m_callbackCond.wakeOne();
                }
            }
            m_socket->close();
            break;
        }
    case RESULT_OPEN_VIDEO:
        m_connectedInternal = true;
        m_callbackCond.wakeAll();

        m_channelProcessed = true;
        m_channelCond.wakeAll();
        break;
    case RESULT_CLOSE_VIDEO:
        m_channelProcessed = true;
        m_channelCond.wakeAll();
        break;
    case RESULT_ALIVECHK:
        {
            if( _result->mResult == false )
            {
                m_connectedInternal = false;
                m_callbackCond.wakeOne();
            }
            break;
        }
    case RESULT_SEARCH_PLAY:
        {
            m_archivePlayProcessing = false;
            if (m_needStop) {
                m_callbackCond.wakeOne();
                return;
            }
            //m_pointsNeedFrame = true;
            if (m_pointsPlayMode) {
                m_curSequence = m_reqSequence;
                m_ppState = PP_GotAnswer;
            }
            else {
                if (m_newPlayCommand.isEmpty())
                    m_curSequence = m_reqSequence;
                else
                    archivePlayInternal(m_newPlayCommand, m_reqSequence);
                m_newPlayCommand.clear();
            }
            break;
        }
    case RESULT_REC_DATE_TIME :
        {
            if( _result->mResult == true )
            {
                int startDate , startTime, endDate, endTime;
                m_ACSStream->getRecordDateTime(startDate, startTime, endDate, endTime);

                QDateTime startDateTime = fromNativeTimestamp(startDate, startTime, 0);
                QDateTime endDateTime = fromNativeTimestamp(endDate, endTime, 0);

                quint8 vMaxHeader[16];
                vMaxHeader[0] = m_curSequence;
                vMaxHeader[1] = VMAXDT_GotArchiveRange;

                vMaxHeader[2] = 0;
                vMaxHeader[3] = 0;
                *(quint32*)(vMaxHeader+4) = 0;
                *(quint32*)(vMaxHeader+8) = startDateTime.toMSecsSinceEpoch()/1000;
                *(quint32*)(vMaxHeader+12) = endDateTime.toMSecsSinceEpoch()/1000;
                m_socket->send(vMaxHeader, sizeof(vMaxHeader));
            }
            break;
        }

    case  RESULT_REC_INFO_MONTH:
        {
            if (_result->mResult == true && m_ACSStream && !m_monthRequests.isEmpty())
            {
                int monthNum = m_monthRequests.dequeue();
                int monthInfo = m_ACSStream->getRecordMonth(monthNum + 1);

                QDate monthDate(monthNum/10000, (monthNum%10000)/100, 1);
                qDebug() << "month=" << monthNum + 1 << "info=" << monthInfo;

                quint8 vMaxHeader[16];
                vMaxHeader[0] = m_curSequence;
                vMaxHeader[1] = VMAXDT_GotMonthInfo;

                vMaxHeader[2] = 0;
                vMaxHeader[3] = 0;
                *(quint32*)(vMaxHeader+4) = 0;
                *(quint32*)(vMaxHeader+8) = monthInfo;
                *(quint32*)(vMaxHeader+12) = monthNum;
                m_socket->send(vMaxHeader, sizeof(vMaxHeader));
            }

            break;
        }

    case  RESULT_REC_TIME_TABLE:
        {
            if (_result->mResult == true && m_ACSStream && !m_daysRequests.isEmpty())
            {
                int dayNum = m_daysRequests.dequeue();
                m_ACSStream->getRecordDayInfo(dayNum, &recordedDayInfo[0][0]);

                quint8 vMaxHeader[16];
                vMaxHeader[0] = m_curSequence;
                vMaxHeader[1] = VMAXDT_GotDayInfo;

                vMaxHeader[2] = 0;
                vMaxHeader[3] = 0;
                *(quint32*)(vMaxHeader+4) = sizeof(recordedDayInfo);
                *(quint32*)(vMaxHeader+8) = 0;
                *(quint32*)(vMaxHeader+12) = dayNum;
                m_socket->send(vMaxHeader, sizeof(vMaxHeader));
                m_socket->send(recordedDayInfo, sizeof(recordedDayInfo));
            }

            break;
        }
    }
}

void QnVMax480Provider::receiveAudioStream(S_ACS_AUDIO_STREAM* _stream)
{
    QMutexLocker lock(&m_callbackMutex);

    if (m_curSequence != m_reqSequence)
        return;

    ACS_audio_codec::A_CODEC_TYPE codec = (ACS_audio_codec::A_CODEC_TYPE) _stream->mSrcType;

    QDateTime packetTimestamp = fromNativeTimestamp(_stream->mDate, _stream->mTime, _stream->mMillisec);

    quint8 VMaxHeader[16];
    VMaxHeader[0] = m_reqSequence;
    VMaxHeader[1] = VMAXDT_GotAudioPacket;
    VMaxHeader[2] = (quint8) _stream->mSrcType;
    VMaxHeader[3] = _stream->mCh << 4;
    *(quint32*)(VMaxHeader+4) = _stream->mSrcSize;
    *(quint64*)(VMaxHeader+8) = packetTimestamp.toMSecsSinceEpoch() * 1000;

    quint8 VMaxAudioHeader[4];

    VMaxAudioHeader[0] = (quint8) _stream->mChannels;
    VMaxAudioHeader[1] = (quint8) _stream->mSamplingbits;
    *(quint16*)(VMaxAudioHeader+2) = (quint16) _stream->mSamplingrate;

    //m_socket->send(VMaxHeader, sizeof(VMaxHeader));
    //m_socket->send(VMaxAudioHeader, sizeof(VMaxAudioHeader));
    //m_socket->send(_stream->mSrc, _stream->mSrcSize);
}
