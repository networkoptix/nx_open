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
static const int DEFAULT_ISD_PORT = 80;
static const int ISD_HTTP_REQUEST_TIMEOUT = 6000;
static const int PRIMARY_ENCODER_NUMBER = 0;
static const int SECONDARY_ENCODER_NUMBER = 1;

static QStringList getValues(const QString& line)
{
    QStringList result;

    int index = line.indexOf(QLatin1Char('='));

    if (index < 0)
        return result;

    QString values = line.mid(index+1);

    return values.split(QLatin1Char(','));
}

static bool sizeCompare(const QSize &s1, const QSize &s2)
{
    return s1.width() > s2.width();
}


MediaEncoder::MediaEncoder(
    CameraManager* const cameraManager,
    unsigned int encoderNum )
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager ),
    m_encoderNum( encoderNum ),
    m_motionMask( nullptr ),
    m_fpsListRead( false ),
    m_resolutionListRead( false ),
    m_audioEnabled( false )
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

int MediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    if( m_encoderNum == PRIMARY_ENCODER_NUMBER )
        *maxBitrate = 15*1024;
    else if( m_encoderNum == SECONDARY_ENCODER_NUMBER )
        *maxBitrate = 2*1024;
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
    *selectedFps = fps;
    return setCameraParam( QString::fromLatin1("VideoInput.1.h264.%1.FrameRate=%2\r\n").arg(m_encoderNum+1).arg(fps) );
}

int MediaEncoder::setBitrate( int bitrateKbps, int* selectedBitrateKbps )
{
    int maxBitrate = 0;
    getMaxBitrate( &maxBitrate );
    if( bitrateKbps > maxBitrate )
        bitrateKbps = maxBitrate;
    *selectedBitrateKbps = bitrateKbps;
    return setCameraParam( QString::fromLatin1("VideoInput.1.h264.%1.BitRate=%2\r\n").arg(m_encoderNum+1).arg(bitrateKbps) );
}

nxcip::StreamReader* MediaEncoder::getLiveStreamReader()
{
    if( !m_streamReader.get() ) {
        m_streamReader.reset( new StreamReader(
            &m_refManager,
            m_encoderNum,
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
    if( !m_streamReader.get() ) {
        m_streamReader.reset( new StreamReader(
            &m_refManager,
            m_encoderNum,
            m_cameraManager->info().uid ) );
        if (m_motionMask)
            m_streamReader->setMotionMask((const uint8_t*) m_motionMask->data());
        m_streamReader->setAudioEnabled( m_audioEnabled );
    }

    return m_streamReader->getAudioFormat( audioFormat );
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
    int resIndex = 0;
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
    }

    m_resolutionListRead = true;
    return nxcip::NX_NO_ERROR;
}
