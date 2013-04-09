/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "camera_plugin_qt_wrapper.h"


namespace nxcip_qt
{
    CommonInterfaceRefManager::CommonInterfaceRefManager( nxpl::NXPluginInterface* const intf )
    :
        m_intf( intf )
    {
    }

    CommonInterfaceRefManager::CommonInterfaceRefManager( const CommonInterfaceRefManager& origin )
    :
        m_intf( origin.m_intf )
    {
        m_intf->addRef();
    }

    CommonInterfaceRefManager::~CommonInterfaceRefManager()
    {
        m_intf->releaseRef();
    }


    CameraDiscoveryManager::CameraDiscoveryManager( nxcip::CameraDiscoveryManager* const intf )
    :
        CommonInterfaceRefManager( intf ),
        m_intf( intf ),
        m_texBuf( new char[nxcip::MAX_TEXT_LEN] )
    {
    }

    CameraDiscoveryManager::CameraDiscoveryManager( const CameraDiscoveryManager& right )
    :
        CommonInterfaceRefManager( right ),
        m_intf( right.m_intf ),
        m_texBuf( new char[nxcip::MAX_TEXT_LEN] )
    {
    }

    CameraDiscoveryManager::~CameraDiscoveryManager()
    {
        delete[] m_texBuf;
    }

    //!See nxcip::CameraDiscoveryManager::getVendorName
    QString CameraDiscoveryManager::getVendorName() const
    {
        m_intf->getVendorName( m_texBuf );
        return QString::fromUtf8( m_texBuf );
    }

    //!See nxcip::CameraDiscoveryManager::findCameras
    int CameraDiscoveryManager::findCameras( QVector<nxcip::CameraInfo>* const cameras, const QString& localInterfaceIPAddr )
    {
        cameras->resize( nxcip::CameraDiscoveryManager::CAMERA_INFO_ARRAY_SIZE );
        const QByteArray& localInterfaceIPAddrUtf8 = localInterfaceIPAddr.toUtf8();
        int result = m_intf->findCameras( cameras->data(), localInterfaceIPAddrUtf8.data() );
        cameras->resize( result > 0 ? result : 0 );
        return result;
    }

    //!See nxcip::CameraDiscoveryManager::checkHostAddress
    int CameraDiscoveryManager::checkHostAddress(
        QVector<nxcip::CameraInfo>* const cameras,
        const QString& url,
        const QString& login,
        const QString& password )
    {
        cameras->resize( nxcip::CameraDiscoveryManager::CAMERA_INFO_ARRAY_SIZE );
        const QByteArray& urlUtf8 = url.toUtf8();
        const QByteArray& loginUtf8 = login.toUtf8();
        const QByteArray& passwordUtf8 = password.toUtf8();
        int result = m_intf->checkHostAddress( cameras->data(), urlUtf8.data(), loginUtf8.data(), passwordUtf8.data() );
        cameras->resize( result > 0 ? result : 0 );
        return result;
    }

    int CameraDiscoveryManager::fromMDNSData( const QByteArray& mdnsResponsePacket, const QHostAddress& discoveredAddress, nxcip::CameraInfo* cameraInfo )
    {
        QByteArray discoveredAddressUtf8 = discoveredAddress.toString().toUtf8();
        return m_intf->fromMDNSData(
            discoveredAddressUtf8.data(),
            (const unsigned char*)mdnsResponsePacket.data(),
            mdnsResponsePacket.size(),
            cameraInfo ) != 0;
    }

    //!See nxcip::CameraDiscoveryManager::fromUpnpData
    bool CameraDiscoveryManager::fromUpnpData( const QByteArray& upnpXMLData, nxcip::CameraInfo* cameraInfo )
    {
        return m_intf->fromUpnpData( upnpXMLData.data(), upnpXMLData.size(), cameraInfo ) != 0;
    }

    //!See nxcip::CameraDiscoveryManager::createCameraManager
    nxcip::BaseCameraManager* CameraDiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
    {
        return m_intf->createCameraManager( info );
    }


    ////////////////////////////////////////////////////////////
    //// CameraMediaEncoder class
    ////////////////////////////////////////////////////////////
    CameraMediaEncoder::CameraMediaEncoder( nxcip::CameraMediaEncoder* const intf )
    :
        CommonInterfaceRefManager( intf ),
        m_intf( intf ),
        m_textBuf( new char[nxcip::MAX_TEXT_LEN] )
    {
    }

    CameraMediaEncoder::~CameraMediaEncoder()
    {
        delete[] m_textBuf;
    }

    int CameraMediaEncoder::getMediaUrl( QString* const url ) const
    {
        int result = m_intf->getMediaUrl( m_textBuf );
        if( result == nxcip::NX_NO_ERROR )
            *url = QString::fromUtf8( m_textBuf );
        return result;
    }

    //!See nxcip::BaseCameraManager::getResolutionList
    int CameraMediaEncoder::getResolutionList( QVector<nxcip::ResolutionInfo>* infoList ) const
    {
        infoList->resize( nxcip::CameraMediaEncoder::MAX_RESOLUTION_LIST_SIZE );
        int infoListCount = 0;
        int result = m_intf->getResolutionList( infoList->data(), &infoListCount );
        infoList->resize( infoListCount );
        return result;
    }

    //!See nxcip::BaseCameraManager::getMaxBitrate
    int CameraMediaEncoder::getMaxBitrate( int* maxBitrate ) const
    {
        return m_intf->getMaxBitrate( maxBitrate );
    }

    //!See nxcip::BaseCameraManager::setResolution
    int CameraMediaEncoder::setResolution( const nxcip::Resolution& resolution )
    {
        return m_intf->setResolution( resolution );
    }

    //!See nxcip::BaseCameraManager::setFps
    int CameraMediaEncoder::setFps( const float& fps, float* selectedFps )
    {
        return m_intf->setFps( fps, selectedFps );
    }

    //!See nxcip::BaseCameraManager::setBitrate
    int CameraMediaEncoder::setBitrate( int bitrateKbps, int* selectedBitrateKbps )
    {
        return m_intf->setBitrate( bitrateKbps, selectedBitrateKbps );
    }


    ////////////////////////////////////////////////////////////
    //// BaseCameraManager class
    ////////////////////////////////////////////////////////////
    BaseCameraManager::BaseCameraManager( nxcip::BaseCameraManager* const intf )
    :
        CommonInterfaceRefManager( intf ),
        m_intf( intf ),
        m_textBuf( new char[nxcip::MAX_TEXT_LEN] )
    {
    }
    
    BaseCameraManager::~BaseCameraManager()
    {
        delete[] m_textBuf;
    }

    //!See nxcip::BaseCameraManager::getEncoderCount
    int BaseCameraManager::getEncoderCount( int* encoderCount ) const
    {
        return m_intf->getEncoderCount( encoderCount );
    }

    int BaseCameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
    {
        return m_intf->getEncoder( encoderIndex, encoderPtr );
    }

    int BaseCameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
    {
        return m_intf->getCameraInfo( info );
    }

    //!See nxcip::BaseCameraManager::getCameraCapabilities
    int BaseCameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
    {
        return m_intf->getCameraCapabilities( capabilitiesMask );
    }

    //!See nxcip::BaseCameraManager::setCredentials
    void BaseCameraManager::setCredentials( const QString& username, const QString& password )
    {
        const QByteArray& usernameUtf8 = username.toUtf8();
        const QByteArray& passwordUtf8 = password.toUtf8();
        m_intf->setCredentials( usernameUtf8.data(), passwordUtf8.data() );
    }

    int BaseCameraManager::setAudioEnabled( bool audioEnabled )
    {
        return m_intf->setAudioEnabled( audioEnabled ? 1 : 0 );
    }

    //!See nxcip::BaseCameraManager::getPTZManager
    nxcip::CameraPTZManager* BaseCameraManager::getPTZManager() const
    {
        return m_intf->getPTZManager();
    }

    //!See nxcip::BaseCameraManager::getCameraMotionDataProvider
    nxcip::CameraMotionDataProvider* BaseCameraManager::getCameraMotionDataProvider() const
    {
        return m_intf->getCameraMotionDataProvider();
    }

    //!See nxcip::BaseCameraManager::getCameraRelayIOManager
    nxcip::CameraRelayIOManager* BaseCameraManager::getCameraRelayIOManager() const
    {
        return m_intf->getCameraRelayIOManager();
    }

    QString BaseCameraManager::getErrorString( int errorCode ) const
    {
        m_intf->getErrorString( errorCode, m_textBuf );
        return QString::fromUtf8( m_textBuf );
    }
}
