/**********************************************************
* 3 apr 2013
* akolesnikov
***********************************************************/

#include "camera_manager.h"

#include <cstddef>
#include <cstring>

#include <QtCore/QMutexLocker>

#include "camera_plugin.h"
#include "cam_params.h"
#include "media_encoder.h"
#include "relayio_manager.h"
#include "sync_http_client.h"


namespace{
static const int DEFAULT_HI_STREAM_FRAMERATE = 25;
static const int DEFAULT_LO_STREAM_FRAMERATE = 25;
}

namespace nxcip
{
    bool operator<( const nxcip::Resolution& left, const nxcip::Resolution& right )
    {
        return left.width * left.height < right.width * right.height;
    }

    bool operator>( const nxcip::Resolution& left, const nxcip::Resolution& right )
    {
        return right < left;
    }
}

CameraManager::ResolutionPair::ResolutionPair()
{
}

CameraManager::ResolutionPair::ResolutionPair(
    nxcip::Resolution _hiRes,
    nxcip::Resolution _loRes )
:
    hiRes( _hiRes ),
    loRes( _loRes )
{
}

bool CameraManager::ResolutionPair::operator<( const ResolutionPair& right ) const
{
    //TODO/IMPL pair with both resolutions is always greater than pair with only high resolution
        //Required only if we want to use max primary resolution disabling secondary stream

    if( hiRes < right.hiRes )
        return true;
    if( right.hiRes < hiRes )
        return false;
    return loRes < right.loRes;
}

bool CameraManager::ResolutionPair::operator>( const ResolutionPair& right ) const
{
    return right < *this;
}


CameraManager::CameraManager( const nxcip::CameraInfo& info )
:
    m_refManager( this ),
    m_pluginRef( CameraPlugin::instance() ),
    m_info( info ),
    m_audioEnabled( false ),
    m_relayIOInfoRead( false ),
    m_cameraCapabilities( 0 ),
    m_hiStreamMaxBitrateKbps( 0 ),
    m_loStreamMaxBitrateKbps( 0 ),
    m_cameraOptionsRead( false ),
    m_forceSecondaryStream( true ),
    m_rtspPort( 554 )
{
    m_credentials.setUser( QString::fromUtf8(m_info.defaultLogin) );
    m_credentials.setPassword( QString::fromUtf8(m_info.defaultPassword) );
    m_encoders.resize( 2 ); //TODO all cameras support dual streaming???

    m_cameraCapabilities |= nxcip::BaseCameraManager::sharePixelsCapability;
    m_cameraCapabilities |= nxcip::BaseCameraManager::fixedQualityCapability;
}

CameraManager::~CameraManager()
{
    for( std::vector<MediaEncoder*>::size_type
        i = 0;
        i < m_encoders.size();
        ++i )
    {
        if( m_encoders[i] )
        {
            m_encoders[i]->releaseRef();
            m_encoders[i] = NULL;
        }
    }
}

void* CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager, sizeof(nxcip::IID_BaseCameraManager) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int CameraManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int CameraManager::releaseRef()
{
    return m_refManager.releaseRef();
}

//!Implementation of nxcip::BaseCameraManager::getEncoderCount
int CameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = m_encoders.size();
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    if( encoderIndex < 0 || encoderIndex >= (int)m_encoders.size() )
        return nxcip::NX_INVALID_ENCODER_NUMBER;

    if( !m_encoders[encoderIndex] )
        m_encoders[encoderIndex] = new MediaEncoder( this, encoderIndex );
    m_encoders[encoderIndex]->addRef();
    *encoderPtr = m_encoders[encoderIndex];
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
{
    //int result = updateCameraInfo();
    //if( result )
    //    return result;

    *info = m_info;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
{
    {
        QMutexLocker lk( &m_mutex );
        if( !m_cameraOptionsRead )
        {
            const int errCode = const_cast<CameraManager*>(this)->readCameraOptions();
            if( errCode != nxcip::NX_NO_ERROR )
                return errCode;
        }
    }

    *capabilitiesMask = m_cameraCapabilities;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::setCredentials
void CameraManager::setCredentials( const char* username, const char* password )
{
    if( username )
        m_credentials.setUser( QString::fromUtf8(username) );
    if( password )
        m_credentials.setPassword( QString::fromUtf8(password) );
}

//!Implementation of nxcip::BaseCameraManager::setAudioEnabled
int CameraManager::setAudioEnabled( int audioEnabled )
{
    m_audioEnabled = audioEnabled != 0;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getPTZManager
nxcip::CameraPtzManager* CameraManager::getPtzManager() const
{
    {
        QMutexLocker lk( &m_mutex );
        if( !m_cameraOptionsRead )
        {
            const int errCode = const_cast<CameraManager*>(this)->readCameraOptions();
            if( errCode != nxcip::NX_NO_ERROR )
                return nullptr;
        }
    }

    if( !(m_cameraCapabilities & nxcip::BaseCameraManager::ptzCapability) )
        return nullptr;

    if( !m_ptzManager.get() )
        m_ptzManager.reset( new PtzManager(const_cast<CameraManager*>(this), m_cameraParamList, m_paramOptions) );
    m_ptzManager->addRef();
    return m_ptzManager.get();
}

nxcip::CameraMotionDataProvider* CameraManager::getCameraMotionDataProvider() const
{
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
{
    {
        QMutexLocker lk( &m_mutex );
        if( !m_cameraOptionsRead )
        {
            const int errCode = const_cast<CameraManager*>(this)->readCameraOptions();
            if( errCode != nxcip::NX_NO_ERROR )
                return nullptr;
        }
    }

    if( !((m_cameraCapabilities & nxcip::BaseCameraManager::relayInputCapability) ||
          (m_cameraCapabilities & nxcip::BaseCameraManager::relayOutputCapability)) )
    {
        return nullptr;
    }

    if( !m_relayIOManager.get() )
        m_relayIOManager.reset( new RelayIOManager(const_cast<CameraManager*>(this), m_relayParamsStr) );
    m_relayIOManager->addRef();
    return m_relayIOManager.get();
}

//!Implementation of nxcip::BaseCameraManager::getErrorString
void CameraManager::getLastErrorString( char* errorString ) const
{
    //TODO/IMPL
    errorString[0] = '\0';
}

const nxcip::CameraInfo& CameraManager::cameraInfo() const
{
    return m_info;
}

int CameraManager::apiPort() const
{
    return DEFAULT_CAMERA_API_PORT;
}

nxcip::CameraInfo& CameraManager::cameraInfo()
{
    return m_info;
}

const QAuthenticator& CameraManager::credentials() const
{
    return m_credentials;
}

template<class ParamAssignFunc>
int CameraManager::readCameraParam( ParamAssignFunc func ) const
{
    QMutexLocker lk( &m_mutex );
    if( !m_cameraOptionsRead )
    {
        const int errCode = const_cast<CameraManager*>(this)->readCameraOptions();
        if( errCode != nxcip::NX_NO_ERROR )
            return errCode;
    }
    func();
    return nxcip::NX_NO_ERROR;
}

int CameraManager::hiStreamResolutions( const std::vector<nxcip::ResolutionInfo>*& resList ) const
{
    return readCameraParam( [this, &resList](){ resList = &m_hiStreamResolutions; } );
}

int CameraManager::loStreamResolutions( const std::vector<nxcip::ResolutionInfo>*& resList ) const
{
    return readCameraParam( [this, &resList](){ resList = &m_loStreamResolutions; } );
}

int CameraManager::hiStreamMaxBitrateKbps( int& bitrate ) const
{
    return readCameraParam( [this, &bitrate](){ bitrate = m_hiStreamMaxBitrateKbps; } );
}

int CameraManager::loStreamMaxBitrateKbps( int& bitrate ) const
{
    return readCameraParam( [this, &bitrate](){ bitrate = m_loStreamMaxBitrateKbps; } );
}

int CameraManager::getRtspPort( int& rtspPort ) const
{
    return readCameraParam( [this, &rtspPort](){ rtspPort = m_rtspPort; } );
}

bool CameraManager::isAudioEnabled() const
{
    return m_audioEnabled;
}

nxpt::CommonRefManager* CameraManager::refManager()
{
    return &m_refManager;
}

int CameraManager::setResolution( int encoderNum, const nxcip::Resolution& resolution )
{
    QMutexLocker lk( &m_mutex );

    if( !m_cameraOptionsRead )
    {
        const int errCode = const_cast<CameraManager*>(this)->readCameraOptions();
        if( errCode != nxcip::NX_NO_ERROR )
            return errCode;
    }

    QStringList newCurrentResolutionCoded = m_currentResolutionCoded;
    if( encoderNum+1 >= newCurrentResolutionCoded.size() )
        return nxcip::NX_UNSUPPORTED_RESOLUTION;
    newCurrentResolutionCoded[encoderNum+1] = getResolutionCode( resolution );

    //TODO/IMPL validating resolution
    ResolutionPair newRes;
    parseResolutionStr( newCurrentResolutionCoded[1].toLatin1(), &newRes.hiRes );
    parseResolutionStr( newCurrentResolutionCoded[2].toLatin1(), &newRes.loRes );
    if( m_supportedResolutions.find(newRes) == m_supportedResolutions.end() )
        return nxcip::NX_UNSUPPORTED_RESOLUTION;

    if( m_currentResolutionCoded == newCurrentResolutionCoded )
        return nxcip::NX_NO_ERROR;

    SyncHttpClient http(
        CameraPlugin::instance()->networkAccessManager(),
        m_info.url,
        DEFAULT_CAMERA_API_PORT,
        m_credentials );
    const int errorCode = updateCameraParameter( &http, "Image.I0.Appearance.Resolution", newCurrentResolutionCoded.join(',').toLatin1() );
    if( errorCode != nxcip::NX_NO_ERROR )
        return errorCode;

    m_currentResolutionCoded = newCurrentResolutionCoded;
    return nxcip::NX_NO_ERROR;
}

int CameraManager::doCameraRequest(
    SyncHttpClient* const httpClient,
    const QByteArray& requestStr,
    std::multimap<QByteArray, QByteArray>* const responseParams )
{
    const QNetworkReply::NetworkError errorCode = httpClient->get( QString::fromLatin1("/cgi-bin/%1").arg(QLatin1String(requestStr)).toLatin1() );
    if( errorCode != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;

    if( httpClient->statusCode() != SyncHttpClient::HTTP_OK )
        return nxcip::NX_NETWORK_ERROR;

    if( !responseParams )
        return nxcip::NX_NO_ERROR;

    const QByteArray& responseMsgBody = httpClient->readWholeMessageBody();
    const QList<QByteArray>& paramStrList = responseMsgBody.split('\n');
    for( const QByteArray& paramStr: paramStrList )
    {
        const QList<QByteArray>& paramAndValue = paramStr.split('=');
        if( paramAndValue.isEmpty() )
            continue;
        responseParams->insert( std::make_pair( paramAndValue[0].trimmed(), paramAndValue.size() > 1 ? paramAndValue[1].trimmed() : QByteArray() ) );
    }

    return nxcip::NX_NO_ERROR;
}

int CameraManager::fetchCameraParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    QVariant* paramValue )
{
    const QNetworkReply::NetworkError errorCode = httpClient->get( QString::fromLatin1("/cgi-bin/admin/param.cgi?action=list&group=%1").arg(QLatin1String(paramName)).toLatin1() );
    if( errorCode != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;

    if( httpClient->statusCode() == SyncHttpClient::HTTP_OK )
    {
        const QByteArray& body = httpClient->readWholeMessageBody();
        const QList<QByteArray>& paramItems = body.split('=');
        if( paramItems.size() == 2 && paramItems[0] == "root."+paramName )
        {
            *paramValue = QString::fromLatin1(paramItems[1]);   //have to convert to QString to enable auto conversion to int
            return nxcip::NX_NO_ERROR;
        }
        else
        {
            return nxcip::NX_NETWORK_ERROR;
        }
    }
    else
    {
        return nxcip::NX_NETWORK_ERROR;
    }
}

int CameraManager::fetchCameraParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    QByteArray* paramValue )
{
    QVariant val;
    int status = fetchCameraParameter( httpClient, paramName, &val );
    *paramValue = val.toByteArray().trimmed();
    return status;
}

int CameraManager::fetchCameraParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    QString* paramValue )
{
    QVariant val;
    int status = fetchCameraParameter( httpClient, paramName, &val );
    *paramValue = val.toString().trimmed();
    return status;
}

int CameraManager::fetchCameraParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    unsigned int* paramValue )
{
    QVariant val;
    int status = fetchCameraParameter( httpClient, paramName, &val );
    *paramValue = val.toUInt();
    return status;
}

int CameraManager::fetchCameraParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    int* paramValue )
{
    QVariant val;
    int status = fetchCameraParameter( httpClient, paramName, &val );
    *paramValue = val.toInt();
    return status;
}

int CameraManager::updateCameraParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    const QByteArray& paramValue )
{
    const QNetworkReply::NetworkError errorCode = httpClient->get( QString::fromLatin1("/cgi-bin/admin/param.cgi?action=update&%1=%2").
            arg(QLatin1String(paramName)).arg(QLatin1String(paramValue)).toLatin1() );
    if( errorCode != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;

    if( httpClient->statusCode() == SyncHttpClient::HTTP_OK )
    {
        QByteArray body = httpClient->readWholeMessageBody();
        if( body.trimmed() == "OK" )
            return nxcip::NX_NO_ERROR;
        else
            return nxcip::NX_NETWORK_ERROR;
    }
    else
    {
        return nxcip::NX_NETWORK_ERROR;
    }
}

int CameraManager::readCameraOptions()
{
    // determin camera max resolution
    SyncHttpClient http(
        CameraPlugin::instance()->networkAccessManager(),
        m_info.url,
        DEFAULT_CAMERA_API_PORT,
        m_credentials );

    //reading parameters
    if( http.get( QLatin1String("/cgi-bin/admin/param.cgi?action=list") ) != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;
    if( http.statusCode() != SyncHttpClient::HTTP_OK )
        return http.statusCode() == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;
    m_cameraParamList = http.readWholeMessageBody().split( '\n' );
    bool rtspEnabled = false;
    int inputPortCount = 0;
    int outputPortCount = 0;
    for( const QByteArray& paramVal: m_cameraParamList )
    {
        const QList<QByteArray>& paramValueList = paramVal.split( '=' );
        if( paramValueList.size() < 2 )
            continue;
        if( paramValueList[0] == "root.Network.RTSP.Port" )
            m_rtspPort = paramValueList[1].toInt();
        else if( paramValueList[0] == "root.Network.RTSP.Enabled" )
            rtspEnabled = paramValueList[1] == "yes";
        else if( paramValueList[0] == "root.Image.I0.Appearance.Resolution" )
            m_currentResolutionCoded = QString::fromLatin1( paramValueList[1] ).split(',');
        else if( paramValueList[0] == "root.Input.NbrOfInputs" )
        {
            inputPortCount = paramValueList[1].toInt();
            m_relayParamsStr.push_back( paramVal );
        }
        else if( paramValueList[0].startsWith("root.Input.") )
            m_relayParamsStr.push_back( paramVal );
        else if( paramValueList[0] == "root.Output.NbrOfOutputs" )
        {
            outputPortCount = paramValueList[1].toInt();
            m_relayParamsStr.push_back( paramVal );
        }
        else if( paramValueList[0].startsWith("root.Output.") )
            m_relayParamsStr.push_back( paramVal );
        else if( paramValueList[0] == "root.Properties.PTZ.PTZ" )
            m_cameraCapabilities |= nxcip::BaseCameraManager::ptzCapability;
    }

    if( inputPortCount > 0 )
        m_cameraCapabilities |= nxcip::BaseCameraManager::relayInputCapability;
    if( outputPortCount > 0 )
        m_cameraCapabilities |= nxcip::BaseCameraManager::relayOutputCapability;

    //"/cgi-bin/admin/param.cgi?action=list&group=Image.I0.Appearance.Resolution"
    if( http.get( QLatin1String("/cgi-bin/admin/param.cgi?action=options") ) != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;
    if( http.statusCode() != SyncHttpClient::HTTP_OK )
        return http.statusCode() == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;

    m_paramOptions = http.readWholeMessageBody().split( '\n' );
    int hiStreamMaxFps = DEFAULT_HI_STREAM_FRAMERATE;
    int loStreamMaxFps = DEFAULT_LO_STREAM_FRAMERATE;
    QByteArray resolutionListStr;
    for( const QByteArray& paramVal: m_paramOptions )
    {
        const QList<QByteArray>& paramValueList = paramVal.split( '=' );
        if( paramValueList[0] == "root.Framerate.H264" )
            hiStreamMaxFps = paramValueList[1].mid( paramValueList[1].indexOf('-')+1 ).toInt();
        else if( paramValueList[0] == "root.Framerate.H264_2" )
            loStreamMaxFps = paramValueList[1].mid( paramValueList[1].indexOf('-')+1 ).toInt();
        else if( paramValueList[0] == "root.Image.I0.Appearance.Resolution" )
            resolutionListStr = paramValueList[1];
        else if( paramValueList[0] == "root.Image.I0.Appearance.H264Bitrate" )
            m_hiStreamMaxBitrateKbps = paramValueList[1].mid( paramValueList[1].indexOf('-')+1 ).toInt();
        else if( paramValueList[0] == "root.Image.I0.Appearance.H264_2Bitrate" )
            m_loStreamMaxBitrateKbps = paramValueList[1].mid( paramValueList[1].indexOf('-')+1 ).toInt();
    }

    parseResolutionList( resolutionListStr, hiStreamMaxFps, loStreamMaxFps );
    m_cameraOptionsRead = true;

    return nxcip::NX_NO_ERROR;
}

void CameraManager::parseResolutionList( const QByteArray& resListStr, int hiStreamMaxFps, int loStreamMaxFps )
{
    QList<QByteArray> resList = resListStr.split( '&' );
    //set<pair<hi_stream_res, lo_stream_res> >
    std::set<std::pair<QByteArray, QByteArray> > uniqueResList;
    for( const QByteArray& resStr: resList )
    {
        const QList<QByteArray>& resPartsList = resStr.split( ',' );
        if( resPartsList.size() < 3 )
            continue;
        uniqueResList.insert( std::make_pair( resPartsList[1], resPartsList[2] ) );
    }

    m_supportedResolutions.clear();
    std::set<nxcip::Resolution, std::greater<nxcip::Resolution> > hiStreamUniqueResolutions;
    std::set<nxcip::Resolution, std::greater<nxcip::Resolution> > loStreamUniqueResolutions;
    for( const std::pair<QByteArray, QByteArray>& resStrPair: uniqueResList )
    {
        nxcip::ResolutionInfo hiStreamResInfo;
        hiStreamResInfo.maxFps = hiStreamMaxFps;
        parseResolutionStr( resStrPair.first, &hiStreamResInfo.resolution );

        nxcip::ResolutionInfo loStreamResInfo;
        loStreamResInfo.maxFps = loStreamMaxFps;
        parseResolutionStr( resStrPair.second, &loStreamResInfo.resolution );

        if( m_forceSecondaryStream && (hiStreamResInfo.resolution.width == -1 || loStreamResInfo.resolution.width == -1) )
            continue;   //skipping resolutions with one stream disabled

        m_supportedResolutions.insert( ResolutionPair( hiStreamResInfo.resolution, loStreamResInfo.resolution ) );
        hiStreamUniqueResolutions.insert( hiStreamResInfo.resolution );
        loStreamUniqueResolutions.insert( loStreamResInfo.resolution );
    }

    m_hiStreamResolutions.clear();
    for( const nxcip::Resolution& res: hiStreamUniqueResolutions )
    {
        nxcip::ResolutionInfo resInfo;
        resInfo.resolution = res;
        resInfo.maxFps = hiStreamMaxFps;
        m_hiStreamResolutions.push_back( resInfo );
    }

    m_loStreamResolutions.clear();
    for( const nxcip::Resolution& res: loStreamUniqueResolutions )
    {
        nxcip::ResolutionInfo resInfo;
        resInfo.resolution = res;
        resInfo.maxFps = loStreamMaxFps;
        m_loStreamResolutions.push_back( resInfo );
    }
}

void CameraManager::parseResolutionStr(
    const QByteArray& resStr,
    nxcip::Resolution* const resolution )
{
    if( resStr == "qcif" )
    {
        resolution->width = 176;
        resolution->height = 144;
    }
    else if( resStr == "cif" )
    {
        resolution->width = 352;
        resolution->height = 288;
    }
    else if( resStr == "2cif" )
    {
        resolution->width = 704;
        resolution->height = 288;
    }
    else if( resStr == "4cif" )
    {
        resolution->width = 704;
        resolution->height = 576;
    }
    else if( resStr == "d1" )
    {
        resolution->width = 720;
        resolution->height = 576;
    }
    else if( resStr == "720p" )
    {
        resolution->width = 1280;
        resolution->height = 720;
    }
    else if( resStr == "1080p" )
    {
        resolution->width = 1920;
        resolution->height = 1080;
    }
    else if( resStr == "sxga" )
    {
        resolution->width = 1280;
        resolution->height = 1024;
    }
    else if( resStr == "svga" )
    {
        resolution->width = 800;
        resolution->height = 600;
    }
    else if( resStr == "vga" )
    {
        resolution->width = 640;
        resolution->height = 480;
    }
    else if( resStr == "xga" )
    {
        resolution->width = 1024;
        resolution->height = 768;
    }
    else //if( resStr == "disable" )
    {
        resolution->width = -1;
        resolution->height = -1;
    }
}

QByteArray CameraManager::getResolutionCode( const nxcip::Resolution& resolution )
{
    if( resolution.width == 176 && resolution.height == 144 )
        return "qcif";
    else if( resolution.width == 352 && resolution.height == 288 )
        return "cif";
    else if( resolution.width == 704 && resolution.height == 288 )
        return "2cif";
    else if( resolution.width == 704 && resolution.height == 576 )
        return "4cif";
    else if( resolution.width == 720 && resolution.height == 576 )
        return "d1";
    else if( resolution.width == 1280 && resolution.height == 720 )
        return "720p";
    else if( resolution.width == 1920 && resolution.height == 1080 )
        return "1080p";
    else if( resolution.width == 1280 && resolution.height == 1024 )
        return "sxga";
    else if( resolution.width == 800 && resolution.height == 600 )
        return "svga";
    else if( resolution.width == 640 && resolution.height == 480 )
        return "vga";
    else if( resolution.width == 1024 && resolution.height == 768 )
        return "xga";
    else
        return "disable";
}
