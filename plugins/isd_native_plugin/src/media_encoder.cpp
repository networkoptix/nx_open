/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "media_encoder.h"

#include <QtCore/QList>
#include <QtCore/QSize>
#include <QtCore/QStringList>

#include <utils/network/simple_http_client.h>

#include "camera_manager.h"
#include "stream_reader.h"


static const QLatin1String localhost( "127.0.0.1" );
static const int DEFAULT_ISD_PORT = 8127;
static const int ISD_HTTP_REQUEST_TIMEOUT = 6000;
static const int PRIMARY_ENCODER_NUMBER = 0;
static const int SECONDARY_ENCODER_NUMBER = 1;
static const int MAX_FPS = 30;

static QStringList getValues(const QString& line)
{
    QStringList result;

    int index = line.indexOf(QLatin1Char('='));

    if (index < 0)
        return result;

    QString values = line.mid(index+1);

    return values.split(QLatin1Char(','));
}


MediaEncoder::MediaEncoder(
    CameraManager* const cameraManager,
    unsigned int encoderNum
#ifndef NO_ISD_AUDIO
    , const std::unique_ptr<AudioStreamReader>& audioStreamReader
#endif
    )
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager ),
    m_encoderNum( encoderNum ),
#ifndef NO_ISD_AUDIO
    m_audioStreamReader( audioStreamReader ),
#endif
    m_motionMask( nullptr ),
    m_fpsListRead( false ),
    m_resolutionListRead( false ),
    m_audioEnabled( false )
#ifdef BITRATE_IS_PER_GOP
    , m_currentFps( MAX_FPS )
    , m_desiredStreamBitrateKbps( 0 )
    , m_fpsUsedToCalcBitrate( 0.0 )
#endif
{
}

MediaEncoder::~MediaEncoder()
{
    if (m_motionMask)
        m_motionMask->releaseRef();
}

void* MediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder2, sizeof(nxcip::IID_CameraMediaEncoder2) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder2*>(this);
    }
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder, sizeof(nxcip::IID_CameraMediaEncoder) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int MediaEncoder::addRef()
{
    return m_refManager.addRef();
}

unsigned int MediaEncoder::releaseRef()
{
    return m_refManager.releaseRef();
}

int MediaEncoder::getMediaUrl( char* urlBuf ) const
{
    strcpy( urlBuf, m_cameraManager->info().url );
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const
{
    int result = nxcip::NX_NO_ERROR;
    if( !m_fpsListRead )
    {
        result = getSupportedFps();
        if( result != nxcip::NX_NO_ERROR )
            return result;
    }

    if( !m_resolutionListRead )
    {
        result = getSupportedResolution();
        if( result != nxcip::NX_NO_ERROR )
            return result;
    }

    *infoListCount = std::min<size_t>( m_supportedResolutions.size(), nxcip::MAX_RESOLUTION_LIST_SIZE );
    std::vector<nxcip::ResolutionInfo>::iterator endIter = m_supportedResolutions.begin() + *infoListCount;
    std::copy( m_supportedResolutions.begin(), endIter, infoList );

    return result;
}

static const int FULL_HD_LINES = 1080;
static const int _4K_MAX_BITRATE_KBPS = 20*1024;
static const int FULL_HD_MAX_BITRATE = 8*1024;

int MediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    if( !m_resolutionListRead )
    {
        int result = getSupportedResolution();
        if( result != nxcip::NX_NO_ERROR )
            return result;
    }

    if( m_encoderNum == PRIMARY_ENCODER_NUMBER )
        *maxBitrate = m_maxResolution.height > FULL_HD_LINES ? _4K_MAX_BITRATE_KBPS : FULL_HD_MAX_BITRATE;
    else if( m_encoderNum == SECONDARY_ENCODER_NUMBER )
        *maxBitrate = (m_maxResolution.height > FULL_HD_LINES ? _4K_MAX_BITRATE_KBPS : FULL_HD_MAX_BITRATE) / 8;
    else
        *maxBitrate = 0;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setResolution( const nxcip::Resolution& resolution )
{
    return setCameraParam( QString::fromLatin1("VideoInput.1.h264.%1.Resolution=%2x%3\r\n").arg(m_encoderNum+1).arg(resolution.width).arg(resolution.height) );
}

int MediaEncoder::setFps( const float& fps, float* selectedFps )
{
    //std::cout<<"MediaEncoder::setFps. "<<fps<<std::endl;
    *selectedFps = fps;
    int result = setCameraParam( QString::fromLatin1("VideoInput.1.h264.%1.FrameRate=%2\r\n").arg(m_encoderNum+1).arg(fps) );
    if( result )
        return result;
#ifdef BITRATE_IS_PER_GOP
    m_currentFps = *selectedFps;
    //if bitrate has been set previously, updating it, since fps changed
    if( m_desiredStreamBitrateKbps > 0      //bitrate has been set previously
        && *selectedFps > 0
        && m_fpsUsedToCalcBitrate != *selectedFps )
    {
        m_fpsUsedToCalcBitrate = *selectedFps;
        int bitrateKbps = (int64_t)m_desiredStreamBitrateKbps * MAX_FPS / m_fpsUsedToCalcBitrate;
        //std::cout<<"MediaEncoder::setFps. setting bitrate to "<<bitrateKbps<<" Kbps"<<std::endl;
        return setCameraParam( QString::fromLatin1("VideoInput.1.h264.%1.BitRate=%2\r\n").arg(m_encoderNum+1).arg(bitrateKbps) );
    }
    return result;
#else
    return result;
#endif
}

int MediaEncoder::setBitrate( int bitrateKbps, int* selectedBitrateKbps )
{
    int maxBitrate = 0;
    getMaxBitrate( &maxBitrate );
    if( bitrateKbps > maxBitrate )
        bitrateKbps = maxBitrate;
    *selectedBitrateKbps = bitrateKbps;
#ifdef BITRATE_IS_PER_GOP
    if( m_currentFps >= 1.0 )
    {
        //on jaguar bitrate we set with setCameraParam is per 30fps, so have to adjust that
        m_desiredStreamBitrateKbps = bitrateKbps;
        m_fpsUsedToCalcBitrate = m_currentFps;
        bitrateKbps = (int64_t)bitrateKbps * MAX_FPS / m_currentFps;
    }
#endif

    //std::cout<<"MediaEncoder::setBitrate "<<bitrateKbps<<" Kbps"<<std::endl;
    QString result;
    QTextStream t(&result);
    t << "VideoInput.1.h264."<<(m_encoderNum+1)<<".BitrateControl=vbr\r\n";
    t << "VideoInput.1.h264."<<(m_encoderNum+1)<<".BitrateVariableMin=" << (bitrateKbps / 5) << "\r\n";
    t << "VideoInput.1.h264."<<(m_encoderNum+1)<<".BitrateVariableMax=" << (bitrateKbps / 5 * 6) << "\r\n";    //*1.2
    t.flush();
    return setCameraParam( result );
}

nxcip::StreamReader* MediaEncoder::getLiveStreamReader()
{
    if( !m_streamReader.get() ) {
        m_streamReader.reset( new StreamReader(
            &m_refManager,
            m_encoderNum,
#ifndef NO_ISD_AUDIO
            m_audioStreamReader,
#endif
            m_cameraManager->info().uid ) );
        if (m_motionMask)
            m_streamReader->setMotionMask((const uint8_t*) m_motionMask->data());
        m_streamReader->setAudioEnabled( m_audioEnabled );
    }
    m_streamReader->addRef();
    return m_streamReader.get();
}

int MediaEncoder::getAudioFormat( nxcip::AudioFormat* audioFormat ) const
{
#ifndef NO_ISD_AUDIO
    if( !m_audioStreamReader->initializeIfNeeded() )
        return nxcip::NX_IO_ERROR;
    *audioFormat = m_audioStreamReader->getAudioFormat();
    return nxcip::NX_NO_ERROR;
#else
    return nxcip::NX_NOT_IMPLEMENTED;
#endif
    //if( !m_streamReader.get() ) {
    //    m_streamReader.reset( new StreamReader(
    //        &m_refManager,
    //        m_encoderNum,
    //        m_audioStreamBridge,
    //        m_cameraManager->info().uid ) );
    //    if (m_motionMask)
    //        m_streamReader->setMotionMask((const uint8_t*) m_motionMask->data());
    //    m_streamReader->setAudioEnabled( m_audioEnabled );
    //}

    //return m_streamReader->getAudioFormat( audioFormat );
}

void MediaEncoder::setMotionMask( nxcip::Picture* motionMask )
{
    if (m_motionMask)
        m_motionMask->releaseRef();
    m_motionMask = motionMask;
    m_motionMask->addRef();
    if (m_streamReader.get())
        m_streamReader->setMotionMask((const uint8_t*) m_motionMask->data());
}

void MediaEncoder::setAudioEnabled( bool audioEnabled )
{
    m_audioEnabled = audioEnabled;
    if (m_streamReader.get())
        m_streamReader->setAudioEnabled( m_audioEnabled );
}

int MediaEncoder::setCameraParam( const QString& request )
{
    QAuthenticator auth;
    auth.setUser( QString::fromLatin1(m_cameraManager->info().defaultLogin) );
    auth.setPassword( QString::fromLatin1(m_cameraManager->info().defaultPassword) );
    CLSimpleHTTPClient http( localhost, DEFAULT_ISD_PORT, ISD_HTTP_REQUEST_TIMEOUT, auth );
    CLHttpStatus status = http.doPOST( QByteArray("/api/param.cgi"), request );

    return status == CL_HTTP_SUCCESS
        ? nxcip::NX_NO_ERROR
        : nxcip::NX_NETWORK_ERROR;
}

int MediaEncoder::getSupportedFps() const
{
    QAuthenticator auth;
    auth.setUser( QString::fromLatin1(m_cameraManager->info().defaultLogin) );
    auth.setPassword( QString::fromLatin1(m_cameraManager->info().defaultPassword) );

    //reading supported fps list
    CLHttpStatus status = CL_HTTP_SUCCESS;
    const QByteArray& fpses = downloadFile(
        status,
        QString::fromLatin1("api/param.cgi?req=VideoInput.1.h264.%1.FrameRateList").arg(m_encoderNum+1),
        localhost,
        DEFAULT_ISD_PORT,
        ISD_HTTP_REQUEST_TIMEOUT,
        auth );
    if( status != CL_HTTP_SUCCESS )
        return status == CL_HTTP_AUTH_REQUIRED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_NETWORK_ERROR;

    const QStringList& vals = getValues(QLatin1String(fpses));

    foreach( const QString& fpsS, vals )
        m_supportedFpsList.push_back(fpsS.trimmed().toFloat());
    m_fpsListRead = true;

    qSort( m_supportedFpsList.begin(), m_supportedFpsList.end(), qGreater<float>() );
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getSupportedResolution() const
{
    if( !m_fpsListRead )
    {
        int result = getSupportedFps();
        if( result != nxcip::NX_NO_ERROR )
            return result;
    }

    QAuthenticator auth;
    auth.setUser( QString::fromLatin1(m_cameraManager->info().defaultLogin) );
    auth.setPassword( QString::fromLatin1(m_cameraManager->info().defaultPassword) );

    //reading supported resolution list
    CLHttpStatus status = CL_HTTP_SUCCESS;
    const QByteArray& reslst = downloadFile(
        status,
        QString::fromLatin1("api/param.cgi?req=VideoInput.1.h264.%1.ResolutionList").arg(m_encoderNum+1),
        localhost,
        DEFAULT_ISD_PORT,
        ISD_HTTP_REQUEST_TIMEOUT,
        auth );

    if( status == CL_HTTP_AUTH_REQUIRED )
        return status == CL_HTTP_AUTH_REQUIRED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_NETWORK_ERROR;

    const QStringList& vals = getValues( QLatin1String(reslst) );
    m_supportedResolutions.reserve( vals.size() );
    for( QStringList::const_iterator it = vals.begin(); it != vals.end(); ++it )
    {
        const QStringList& wh_s = it->split(QLatin1Char('x'));
        if( wh_s.size() < 2 )
            continue;
        m_supportedResolutions.push_back( nxcip::ResolutionInfo() );
        m_supportedResolutions.back().resolution.width = wh_s.at(0).toInt();
        m_supportedResolutions.back().resolution.height = wh_s.at(1).toInt();
        m_supportedResolutions.back().maxFps = m_supportedFpsList.empty() ? 0 : m_supportedFpsList[0];
        if( m_maxResolution.height < m_supportedResolutions.back().resolution.height )
            m_maxResolution = m_supportedResolutions.back().resolution;
    }

    m_resolutionListRead = true;
    return nxcip::NX_NO_ERROR;
}
