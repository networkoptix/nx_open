#include "vmax480_live_reader.h"
#include "core/resource/network_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "libavcodec/avcodec.h"
#include "acs_codec.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"

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

QnVMax480LiveProvider::QnVMax480LiveProvider(QnResourcePtr dev ):
    CLServerPushStreamreader(dev),
    m_ACSStream(0),
    m_connected(false),
    m_spsPpsWidth(-1),
    m_spsPpsHeight(-1),
    m_spsPpsBufferLen(0),
    m_connectedInternal(false),
    m_internalQueue(16)
{
    m_networkRes = dev.dynamicCast<QnNetworkResource>();
}

QnVMax480LiveProvider::~QnVMax480LiveProvider()
{

}

QnAbstractMediaDataPtr QnVMax480LiveProvider::getNextData()
{
    QnAbstractDataPacketPtr result;
    QTime getTimer;
    getTimer.restart();
    while (!needToStop() && getTimer.elapsed() < MAX_FRAME_DURATION * 2 && !result)
    {
        m_internalQueue.pop(result, 100);

        if (m_ACSStream && m_aliveTimer.elapsed() > 1000) {
            m_ACSStream->checkAlive();
            m_aliveTimer.restart();
        }
    }

    if (!result)
        closeStream();
    return result.dynamicCast<QnAbstractMediaData>();
}

void QnVMax480LiveProvider::openStream()
{
    if (m_connected)
        return;

    create_acs_source(&m_ACSStream);

    //m_ACSStream->setRecvAudioStreamCallback(receiveAudioStramCallback , (long long)this);
    m_ACSStream->setRecvVideoStreamCallback(receiveVideoStramCallback , (long long)this);
    m_ACSStream->setRecvResultCallback(receiveResultCallback , (long long)this);

    QUrl url(getResource()->getUrl());

    S_CONNECT_PARAM	connectParam;
    connectParam.mUrl	=	url.host().toStdWString();
    connectParam.mTcpPort	=	url.port(80);
    connectParam.mUserID	=	m_networkRes->getAuth().user().toStdWString();
    connectParam.mUserPwd	=	m_networkRes->getAuth().password().toStdWString();

    connectParam.mIsLive	=	true;

    //call connect function
    QString gg = QDir::currentPath();
    m_ACSStream->connect(&connectParam);

    m_connected = true;

    //m_ACSStream->checkAlive();
    //m_ACSStream->requestRecordDateTime();
    m_aliveTimer.restart();
}

void QnVMax480LiveProvider::closeStream()
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

bool QnVMax480LiveProvider::isStreamOpened() const
{
    return m_connected;
}

void QnVMax480LiveProvider::receiveAudioStramCallback(PS_ACS_AUDIO_STREAM _stream, long long _user)
{
    QnVMax480LiveProvider* object = (QnVMax480LiveProvider*)_user;
    object->receiveAudioStream(_stream);
}

void QnVMax480LiveProvider::receiveVideoStramCallback(PS_ACS_VIDEO_STREAM _stream, long long _user)
{
    QnVMax480LiveProvider* object = (QnVMax480LiveProvider*)_user;
    object->receiveVideoStream(_stream);
}
void QnVMax480LiveProvider::receiveResultCallback(PS_ACS_RESULT _result, long long _user)
{
    QnVMax480LiveProvider* object = (QnVMax480LiveProvider*)_user;
    object->receiveResult(_result);
}

QDateTime QnVMax480LiveProvider::fromNativeTimestamp(int mDate, int mTime, int mMillisec)
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

void QnVMax480LiveProvider::receiveVideoStream(S_ACS_VIDEO_STREAM* _stream)
{
    QMutexLocker lock(&m_callbackMutex);

    QString tempStr;

    if(!m_connected)
        return;

    QDateTime packetTimestamp = fromNativeTimestamp(_stream->mDate, _stream->mTime, _stream->mMillisec);

    CodecID compressionType = CODEC_ID_NONE;
    switch (_stream->mSrcType)
    {
        case ACS_video_codec::CODEC_VSTREAM_H264:
            compressionType = CODEC_ID_H264;
            break;
        case ACS_video_codec::CODEC_VSTREAM_JPEG:
            compressionType = CODEC_ID_MJPEG;
            break;
        case ACS_video_codec::CODEC_VSTREAM_MPEG4:
            compressionType = CODEC_ID_MPEG4;
            break;
        default:
            qDebug() << "Got video packet with unknown codec " << _stream->mSrcType << "from VMax DVR" << m_resource->getUrl();
            return;
    }

    if (_stream->mCh == m_networkRes->getChannel())
    {
        if (_stream->mWidth != m_spsPpsWidth || _stream->mHeight != m_spsPpsHeight) {
            m_spsPpsBufferLen = create_vmax_sps_pps(_stream->mWidth, _stream->mHeight, m_spsPpsBuffer, sizeof(m_spsPpsBuffer));
            m_spsPpsWidth = _stream->mWidth;
            m_spsPpsHeight = _stream->mHeight;
        }
        bool isIFrame = _stream->mSrcSize >= 5 && (_stream->mSrc[4] & 0x1f) == 5;

        QnCompressedVideoData* videoData = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, _stream->mSrcSize + (isIFrame ? m_spsPpsBufferLen : 0));
        videoData->width = _stream->mWidth;
        videoData->height = _stream->mHeight;
        if (isIFrame) {
            videoData->flags |= AV_PKT_FLAG_KEY;
            videoData->data.write((const char*) m_spsPpsBuffer, m_spsPpsBufferLen);
        }
        videoData->data.write((const char*) _stream->mSrc, _stream->mSrcSize);
        videoData->timestamp = qnSyncTime->currentUSecsSinceEpoch(); //packetTimestamp.toMSecsSinceEpoch() * 1000;
        videoData->compressionType = compressionType;
        
        m_internalQueue.push(QnCompressedVideoDataPtr(videoData));
        
        //videoData->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        //if (dataCanBeAccepted())
        //    putData(QnCompressedVideoDataPtr(videoData));
    }
    else {
        // other channel
        int gg = 4;
    }
}

void QnVMax480LiveProvider::receiveResult(S_ACS_RESULT* _result)
{
    QString gg = QDir::currentPath();

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
                openChannel = 1 << m_networkRes->getChannel();
                m_ACSStream->openChannel(openChannel);
                //m_ACSStream->openAudioChannel(openChannel);					
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
                    m_resource->setStatus(QnResource::Unauthorized); //OutputDebugString(_T("Not Authorized User\n"));
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

void QnVMax480LiveProvider::receiveAudioStream(S_ACS_AUDIO_STREAM* _stream)
{
    QMutexLocker lock(&m_callbackMutex);

}

void QnVMax480LiveProvider::beforeRun()
{
    msleep(300);
}

void QnVMax480LiveProvider::afterRun()
{
    msleep(300);
}

void QnVMax480LiveProvider::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnVMax480LiveProvider::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

