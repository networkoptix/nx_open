#include "vmax480_live_reader.h"
#include "core/resource/network_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "libavcodec/avcodec.h"
#include "acs_codec.h"

#pragma comment(lib, "c:/develop/netoptix_vmax/vmax/bin/lib/Multithreaded_Debug_DLL/acs_stream_source_mdd.lib")
#pragma comment(lib, "c:/develop/netoptix_vmax/vmax/bin/lib/Multithreaded_Debug_DLL/acs_codec_mdd.lib")
#pragma comment(lib, "c:/develop/netoptix_vmax/vmax/bin/lib/Multithreaded_Debug_DLL/acs_post_processing_mdd.lib")
#pragma comment(lib, "c:/develop/netoptix_vmax/vmax/bin/lib/Multithreaded_Debug_DLL/acs_export_mdd.lib")


// ----------------------------------- QnVMax480LiveProvider -----------------------

QnVMax480LiveProvider::QnVMax480LiveProvider(QnResourcePtr dev ):
    CLServerPushStreamreader(dev),
    m_ACSStream(0),
    m_connected(false)
{
    m_networkRes = dev.dynamicCast<QnNetworkResource>();
}

QnVMax480LiveProvider::~QnVMax480LiveProvider()
{

}

QnAbstractMediaDataPtr QnVMax480LiveProvider::getNextData()
{
    return QnAbstractMediaDataPtr();
}

void QnVMax480LiveProvider::openStream()
{
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
    m_ACSStream->connect(&connectParam);

    m_connected = true;
}

void QnVMax480LiveProvider::closeStream()
{
    destroy_acs_source(&m_ACSStream);
    m_connected = false;
}

bool QnVMax480LiveProvider::isStreamOpened() const
{
    return m_connected;
}

void QnVMax480LiveProvider::updateStreamParamsBasedOnQuality()
{
    // todo: implement me
}

void QnVMax480LiveProvider::updateStreamParamsBasedOnFps()
{
    // todo: implement me
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

    QnCompressedVideoData* videoData = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, _stream->mSrcSize);
    videoData->width = _stream->mWidth;
    videoData->height = _stream->mHeight;
    videoData->data.write((const char*) _stream->mSrc, _stream->mSrcSize);
    videoData->timestamp = packetTimestamp.toMSecsSinceEpoch() * 1000;
}


void QnVMax480LiveProvider::receiveResult(S_ACS_RESULT* _result)
{
    int gg = 4;
    switch(_result->mType)
    {
    case RESULT_CONNECT:
            gg = 4;
            break;
    case RESULT_LOGIN:
        {
            if( _result->mResult == true )
            {
                int openChannel = 0;

                for( int i = 0 ; i < 16;i++)
                {
                    openChannel |= (1<<i);
                }
                m_ACSStream->openChannel(openChannel);
                m_ACSStream->openAudioChannel(openChannel);					
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
                    ; //OutputDebugString(_T("connection failed or disconnected\n"));
            }
            m_connected = false;
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
                    m_ACSStream->requestPlayMode(ACS_stream_source::FORWARDPLAY, 1, startDate, startTime, false);
                    //mACSStream->requestrandom_seek(65535,ACS_stream_source::FORWARDPLAY, 1, startDate, startTime, false );
                }

            }
            break;
        }

    case  RESULT_REC_INFO_MONTH:
        {
            int recordInfoMonth;
            if (_result->mResult == true)
            {
                if( m_ACSStream )
                {
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

}
