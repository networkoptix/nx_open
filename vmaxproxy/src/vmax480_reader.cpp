#include "vmax480_reader.h"
#include "utils/network/socket.h"
#include "acs_codec.h"

#pragma comment(lib, "c:/develop/netoptix_vmax/vmax/bin/lib/Multithreaded_Debug_DLL/acs_stream_source_mdd.lib")
#pragma comment(lib, "c:/develop/netoptix_vmax/vmax/bin/lib/Multithreaded_Debug_DLL/acs_codec_mdd.lib")
#pragma comment(lib, "c:/develop/netoptix_vmax/vmax/bin/lib/Multithreaded_Debug_DLL/acs_post_processing_mdd.lib")
#pragma comment(lib, "c:/develop/netoptix_vmax/vmax/bin/lib/Multithreaded_Debug_DLL/acs_export_mdd.lib")


#include "plugins/resources/arecontvision/tools/nalconstructor.cpp"

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


// ----------------------------------- QnVMax480LiveProvider -----------------------

QnVMax480Provider::QnVMax480Provider(TCPSocket* socket):
    m_ACSStream(0),
    m_connected(false),
    m_spsPpsWidth(-1),
    m_spsPpsHeight(-1),
    m_spsPpsBufferLen(0),
    m_connectedInternal(false),
    m_internalQueue(16),
    m_socket(socket),
    m_channelNum(0),
    m_sequence(0)
{
}

QnVMax480Provider::~QnVMax480Provider()
{

}

void QnVMax480Provider::connect(VMaxParamList params, quint8 sequence)
{
    if (m_connected)
        return;

    create_acs_source(&m_ACSStream);

    m_ACSStream->setRecvAudioStreamCallback(receiveAudioStramCallback , (long long)this);
    m_ACSStream->setRecvVideoStreamCallback(receiveVideoStramCallback , (long long)this);
    m_ACSStream->setRecvResultCallback(receiveResultCallback , (long long)this);

    QUrl url(params.value("url"));
    m_channelNum = url.queryItemValue("channel").toInt();

    S_CONNECT_PARAM	connectParam;
    connectParam.mUrl	=	url.host().toStdWString();
    connectParam.mTcpPort	=	url.port(80);
    connectParam.mUserID	=	QString::fromUtf8(params.value("login")).toStdWString();
    connectParam.mUserPwd	=	QString::fromUtf8(params.value("password")).toStdWString();

    connectParam.mIsLive	=	true;

    //call connect function
    m_sequence = sequence;
    m_ACSStream->connect(&connectParam);

    m_connected = true;

    //m_ACSStream->checkAlive();
    //m_ACSStream->requestRecordDateTime();
    m_aliveTimer.restart();
}

void QnVMax480Provider::disconnect()
{
    if (!m_connected)
        return;


    QMutexLocker lock(&m_callbackMutex);

    if(m_ACSStream) {
        //m_ACSStream->closeChannel(1 << m_networkRes->getChannel());
        m_ACSStream->disconnect();
    }

    if (m_connectedInternal) {
        m_callbackCond.wait(&m_callbackMutex, 100);
        m_connectedInternal = false;
    }

    if(m_ACSStream) {
        m_ACSStream->close();
        destroy_acs_source(&m_ACSStream);
        m_ACSStream = 0;
        m_connected = false;
    }
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

QDateTime QnVMax480Provider::fromNativeTimestamp(int mDate, int mTime, int mMillisec)
{
    int _year	= mDate / 10000;
    int _month  = (mDate % 10000) / 100;
    int _day	= (mDate % 10000) % 100;

    int hour	=	mTime /10000;
    int min		=	(mTime%10000)/100;
    int sec		=	(mTime%10000)%100;
    int milli	=	(mMillisec);

    return QDateTime(QDate(_year, _month, _day), QTime(hour, min, sec, milli));
}

void QnVMax480Provider::receiveVideoStream(S_ACS_VIDEO_STREAM* _stream)
{
    QMutexLocker lock(&m_callbackMutex);

    QString tempStr;

    if(!m_connected)
        return;

    QDateTime packetTimestamp = fromNativeTimestamp(_stream->mDate, _stream->mTime, _stream->mMillisec);

    if (_stream->mWidth != m_spsPpsWidth || _stream->mHeight != m_spsPpsHeight) {
        m_spsPpsBufferLen = create_vmax_sps_pps(_stream->mWidth, _stream->mHeight, m_spsPpsBuffer, sizeof(m_spsPpsBuffer));
        m_spsPpsWidth = _stream->mWidth;
        m_spsPpsHeight = _stream->mHeight;
    }
    bool isIFrame = _stream->mSrcSize >= 5 && (_stream->mSrc[4] & 0x1f) == 5;

    int dataSize = _stream->mSrcSize + (isIFrame ? m_spsPpsBufferLen : 0);

    quint8 VMaxHeader[16];
    VMaxHeader[0] = m_sequence;
    VMaxHeader[1] = VMAXDT_GotVideoPacket;
    VMaxHeader[2] = (quint8) _stream->mSrcType;
    VMaxHeader[3] = (quint8) isIFrame;
    *(quint32*)(VMaxHeader+4) = dataSize;
    //*(quint64*)(VMaxHeader+8) = QDateTime::currentDateTime().toMSecsSinceEpoch()*1000;
    *(quint64*)(VMaxHeader+8) = packetTimestamp.toMSecsSinceEpoch() * 1000;
    m_socket->send(VMaxHeader, sizeof(VMaxHeader));
    if (isIFrame)
        m_socket->send(m_spsPpsBuffer, m_spsPpsBufferLen);
    m_socket->send(_stream->mSrc, _stream->mSrcSize);

}

void QnVMax480Provider::receiveResult(S_ACS_RESULT* _result)
{
    QMutexLocker lock(&m_callbackMutex);

    int openChannel = 0;
    int recordInfoMonth;

    switch(_result->mType)
    {
    case RESULT_CONNECT:
            m_connectedInternal = true;
            break;
    case RESULT_LOGIN:
        {
            if( _result->mResult == true )
            {
                m_ACSStream->requestRecordDateTime();

                m_ACSStream->openChannel(1 << m_channelNum);
                m_ACSStream->openAudioChannel(1 << m_channelNum);
            }
            break;
        }

    case RESULT_ERROR:
        {
            if( _result->mData1 != NULL )
            {
                PS_ERROR_PARAM error = (PS_ERROR_PARAM)(_result->mData1);

                if( error->mErrorCode	== 0xE0000001 )
                    ; //OutputDebugString(_T("Unregister user\n"));
                if( error->mErrorCode	== 0xE0000002 )
                    ; //OutputDebugString(_T("Incorrect password\n"));
                if( error->mErrorCode	== 0xE0000003 )
                    ;
                if( error->mErrorCode	== 0xE0000004 )
                    ; //OutputDebugString(_T("Exceeded visitor\n"));
                if( error->mErrorCode	== 0xEFFFFFFF ) 
                {
                    m_callbackCond.wakeOne();
                    m_connectedInternal = false;
                }
            }
            break;
        }

    case RESULT_ALIVECHK:
        {
            if( _result->mResult == false )
            {
                //OutputDebugString(_T("Disconnected"));
                m_connected = false;
            }
            break;
        }
    case RESULT_SEARCH_PLAY:
        {
            //OutputDebugString(_T("Answer reqeustPlaymode from device.\n"));
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
                vMaxHeader[0] = m_sequence;
                vMaxHeader[1] = VMAXDT_GotArchiveRange;
                
                vMaxHeader[2] = 0;
                vMaxHeader[3] = 0;
                *(quint32*)(vMaxHeader+4) = 0;
                *(quint32*)(vMaxHeader+8) = startDateTime.toMSecsSinceEpoch()/1000;
                *(quint32*)(vMaxHeader+12) = endDateTime.toMSecsSinceEpoch()/1000;
                m_socket->send(vMaxHeader, sizeof(vMaxHeader));

                if( m_ACSStream )
                {

                    //requestrandom_seek(int _eventChannel, ACS_stream_source::PLAYMODE _playmode, int _speed, int _date, int _time) = 0;
                    //mACSStream->openChannel(65535);
                    //m_ACSStream->requestPlayMode(ACS_stream_source::FORWARDPLAY, 1, startDate, startTime, false);
                    //mACSStream->requestrandom_seek(65535,ACS_stream_source::FORWARDPLAY, 1, startDate, startTime, false );
                }

            }
            break;
        }

    case  RESULT_REC_INFO_MONTH:
        {
            if (_result->mResult == true)
            {
                if( m_ACSStream )
                {
                    int gg = 4;
                    /*
                    SYSTEMTIME _stcurtime;
                    m_calendar.GetCurSel(&_stcurtime);
                    int searchDate = _stcurtime.wYear*10000 + _stcurtime.wMonth*100 + 1;
                    recordInfoMonth = m_ACSStream->getRecordMonth(searchDate);

                    OutputDebugString(L"get Days success!!\n");
                    setCalendarDate(recordInfoMonth);
                    */
                }

            }

            break;
        }

    case  RESULT_REC_TIME_TABLE:
        {
            if (_result->mResult == true)
            {
                int gg = 4;
                /*
                if (m_ACSStream)
                {
                    memset(&recordDisplayMode, 0, MAX_CH*(1440+60));
                    m_ACSStream->getRecordDayInfo(mRecDay, &recordDisplayMode[0][0]);
                    OutputDebugString(L"get TimeEvent  success!!\n");
                    setRecData();
                }
                */
            }


            break;
        }
    }
}

void QnVMax480Provider::receiveAudioStream(S_ACS_AUDIO_STREAM* _stream)
{
    QMutexLocker lock(&m_callbackMutex);

    ACS_audio_codec::A_CODEC_TYPE codec = (ACS_audio_codec::A_CODEC_TYPE) _stream->mSrcType;

    QDateTime packetTimestamp = fromNativeTimestamp(_stream->mDate, _stream->mTime, _stream->mMillisec);

    quint8 VMaxHeader[16];
    VMaxHeader[0] = m_sequence;
    VMaxHeader[1] = VMAXDT_GotAudioPacket;
    VMaxHeader[2] = (quint8) _stream->mSrcType;
    VMaxHeader[3] = 0;
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
