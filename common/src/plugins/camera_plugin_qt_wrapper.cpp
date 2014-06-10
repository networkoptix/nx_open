/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "camera_plugin_qt_wrapper.h"

#include <QtCore/QMutexLocker>


namespace nxcip_qt
{
    CameraDiscoveryManager::CameraDiscoveryManager( nxcip::CameraDiscoveryManager* const intf )
    :
        CommonInterfaceRefManager<nxcip::CameraDiscoveryManager>( intf ),
        m_texBuf( new char[nxcip::MAX_TEXT_LEN] )
    {
    }

    CameraDiscoveryManager::CameraDiscoveryManager( const CameraDiscoveryManager& right )
    :
        CommonInterfaceRefManager<nxcip::CameraDiscoveryManager>( right ),
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
        QMutexLocker lk( &m_mutex );
        m_intf->getVendorName( m_texBuf );
        return QString::fromUtf8( m_texBuf );
    }

    //!See nxcip::CameraDiscoveryManager::findCameras
    int CameraDiscoveryManager::findCameras( QVector<nxcip::CameraInfo>* const cameras, const QString& localInterfaceIPAddr )
    {
        cameras->resize( nxcip::CAMERA_INFO_ARRAY_SIZE );
        const QByteArray& localInterfaceIPAddrUtf8 = localInterfaceIPAddr.toUtf8();
        int result = m_intf->findCameras( cameras->data(), localInterfaceIPAddrUtf8.data() );
        cameras->resize( result > 0 ? result : 0 );
        return result;
    }

    //!See nxcip::CameraDiscoveryManager::checkHostAddress
    int CameraDiscoveryManager::checkHostAddress(
        QVector<nxcip::CameraInfo>* const cameras,
        const QString& url,
        const QString* login,
        const QString* password )
    {
        cameras->resize( nxcip::CAMERA_INFO_ARRAY_SIZE );
        const QByteArray& urlUtf8 = url.toUtf8();
        const QByteArray loginUtf8 = login ? login->toUtf8() : QByteArray();
        const QByteArray passwordUtf8 = password ? password->toUtf8() : QByteArray();
        int result = m_intf->checkHostAddress(
            cameras->data(),
            urlUtf8.data(),
            login ? loginUtf8.data() : NULL,
            password ? passwordUtf8.data() : NULL );
        //TODO #ak validating cameraInfo data
        cameras->resize( result > 0 ? result : 0 );
        return result;
    }

    int CameraDiscoveryManager::fromMDNSData( const QByteArray& mdnsResponsePacket, const QHostAddress& discoveredAddress, nxcip::CameraInfo* cameraInfo )
    {
        QByteArray discoveredAddressUtf8 = discoveredAddress.toString().toUtf8();
        //TODO #ak validating cameraInfo data
        return m_intf->fromMDNSData(
            discoveredAddressUtf8.data(),
            (const unsigned char*)mdnsResponsePacket.data(),
            mdnsResponsePacket.size(),
            cameraInfo ) != 0;
    }

    //!See nxcip::CameraDiscoveryManager::fromUpnpData
    bool CameraDiscoveryManager::fromUpnpData( const QByteArray& upnpXMLData, nxcip::CameraInfo* cameraInfo )
    {
        //TODO #ak validating cameraInfo data
        return m_intf->fromUpnpData( upnpXMLData.data(), upnpXMLData.size(), cameraInfo ) != 0;
    }

    //!See nxcip::CameraDiscoveryManager::createCameraManager
    nxcip::BaseCameraManager* CameraDiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
    {
        return m_intf->createCameraManager( info );
    }

    QList<QString> CameraDiscoveryManager::getReservedModelList() const
    {
        QList<QString> modelList;

        //requesting size of list
        int count = 0;
        m_intf->getReservedModelList( NULL, &count );
        if( !count )
            return modelList;

        //preparing temporary buffer
        char** tempModelList = new char*[count];
        for( int i = 0; i < count; ++i )
            tempModelList[i] = new char[nxcip::MAX_MODEL_NAME_SIZE];

        //requesting list
        m_intf->getReservedModelList( tempModelList, &count );
        for( int i = 0; i < count; ++i )
            modelList.push_back( QString::fromUtf8(tempModelList[i]) );

        for( int i = 0; i < count; ++i )
            delete[] tempModelList[i];
        delete[] tempModelList;

        return modelList;
    }

    const CameraDiscoveryManager& CameraDiscoveryManager::operator=( const CameraDiscoveryManager& right )
    {
        CommonInterfaceRefManager<nxcip::CameraDiscoveryManager>::operator=( right );
        return *this;
    }


    ////////////////////////////////////////////////////////////
    //// CameraMediaEncoder class
    ////////////////////////////////////////////////////////////
    CameraMediaEncoder::CameraMediaEncoder( nxcip::CameraMediaEncoder* const intf )
    :
        CommonInterfaceRefManager<nxcip::CameraMediaEncoder>( intf ),
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
        infoList->resize( nxcip::MAX_RESOLUTION_LIST_SIZE );
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
        CommonInterfaceRefManager<nxcip::BaseCameraManager>( intf ),
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
        QMutexLocker lk( &m_mutex );
        return m_intf->getEncoderCount( encoderCount );
    }

    int BaseCameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
    {
        QMutexLocker lk( &m_mutex );
        return m_intf->getEncoder( encoderIndex, encoderPtr );
    }

    int BaseCameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
    {
        QMutexLocker lk( &m_mutex );
        return m_intf->getCameraInfo( info );
    }

    //!See nxcip::BaseCameraManager::getCameraCapabilities
    int BaseCameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
    {
        QMutexLocker lk( &m_mutex );
        return m_intf->getCameraCapabilities( capabilitiesMask );
    }

    //!See nxcip::BaseCameraManager::setCredentials
    void BaseCameraManager::setCredentials( const QString& username, const QString& password )
    {
        QMutexLocker lk( &m_mutex );
        const QByteArray& usernameUtf8 = username.toUtf8();
        const QByteArray& passwordUtf8 = password.toUtf8();
        m_intf->setCredentials( usernameUtf8.data(), passwordUtf8.data() );
    }

    int BaseCameraManager::setAudioEnabled( bool audioEnabled )
    {
        QMutexLocker lk( &m_mutex );
        return m_intf->setAudioEnabled( audioEnabled ? 1 : 0 );
    }

    //!See nxcip::BaseCameraManager::getPtzManager
    nxcip::CameraPtzManager* BaseCameraManager::getPtzManager() const
    {
        QMutexLocker lk( &m_mutex );
        return m_intf->getPtzManager();
    }

    //!See nxcip::BaseCameraManager::getCameraRelayIOManager
    nxcip::CameraRelayIOManager* BaseCameraManager::getCameraRelayIOManager() const
    {
        QMutexLocker lk( &m_mutex );
        return m_intf->getCameraRelayIOManager();
    }

    QString BaseCameraManager::getLastErrorString() const
    {
        QMutexLocker lk( &m_mutex );
        m_intf->getLastErrorString( m_textBuf );
        return QString::fromUtf8( m_textBuf );
    }


    ////////////////////////////////////////////////////////////
    //// CameraRelayIOManager class
    ////////////////////////////////////////////////////////////

    CameraRelayIOManager::CameraRelayIOManager( nxcip::CameraRelayIOManager* const intf )
    :
        CommonInterfaceRefManager<nxcip::CameraRelayIOManager>( intf ),
        m_idsList( NULL ),
        m_textBuf( new char[nxcip::MAX_TEXT_LEN] )
    {
        m_idsList = new char*[nxcip::MAX_RELAY_PORT_COUNT];
        for( int i = 0; i < nxcip::MAX_RELAY_PORT_COUNT; ++i )
        {
            m_idsList[i] = new char[nxcip::MAX_ID_LEN];
            m_idsList[i][0] = 0;
        }
    }

    CameraRelayIOManager::~CameraRelayIOManager()
    {
        for( int i = 0; i < nxcip::MAX_RELAY_PORT_COUNT; ++i )
            delete[] m_idsList[i];
        delete[] m_idsList;
        m_idsList = NULL;

        delete[] m_textBuf;
        m_textBuf = NULL;
    }

    //!See nxcip::CameraRelayIOManager::getRelayOutputList
    int CameraRelayIOManager::getRelayOutputList( QStringList* const ids ) const
    {
        int idNum = 0;
        int result = m_intf->getRelayOutputList( m_idsList, &idNum );
        if( result )
            return result;
        if( idNum > nxcip::MAX_RELAY_PORT_COUNT )
            return nxcip::NX_UNDEFINED_BEHAVOUR;

        for( int i = 0; i < idNum; ++i )
            ids->push_back( QString::fromUtf8(m_idsList[i], std::min<size_t>(strlen(m_idsList[i]), nxcip::MAX_ID_LEN) ) );
        return result;
    }

    //!See nxcip::CameraRelayIOManager::getInputPortList
    int CameraRelayIOManager::getInputPortList( QStringList* const ids ) const
    {
        int idNum = 0;
        int result = m_intf->getInputPortList( m_idsList, &idNum );
        if( result )
            return result;
        if( idNum > nxcip::MAX_RELAY_PORT_COUNT )
            return nxcip::NX_UNDEFINED_BEHAVOUR;

        for( int i = 0; i < idNum; ++i )
            ids->push_back( QString::fromUtf8(m_idsList[i], std::min<size_t>(strlen(m_idsList[i]), nxcip::MAX_ID_LEN) ) );
        return result;
    }

    //!See nxcip::CameraRelayIOManager::setRelayOutputState
    int CameraRelayIOManager::setRelayOutputState(
        const QString& outputID,
        bool activate,
        unsigned int autoResetTimeoutMS )
    {
        const QByteArray& outputIDUtf8 = outputID.toUtf8();
        return m_intf->setRelayOutputState( outputIDUtf8.data(), activate ? 1 : 0, autoResetTimeoutMS );
    }

    //!See nxcip::CameraRelayIOManager::startInputPortMonitoring
    int CameraRelayIOManager::startInputPortMonitoring()
    {
        return m_intf->startInputPortMonitoring();
    }

    //!See nxcip::CameraRelayIOManager::stopInputPortMonitoring
    void CameraRelayIOManager::stopInputPortMonitoring()
    {
        return m_intf->stopInputPortMonitoring();
    }

    //!See nxcip::CameraRelayIOManager::registerEventHandler
    void CameraRelayIOManager::registerEventHandler( nxcip::CameraInputEventHandler* handler )
    {
        m_intf->registerEventHandler( handler );
    }

    //!See nxcip::CameraRelayIOManager::unregisterEventHandler
    void CameraRelayIOManager::unregisterEventHandler( nxcip::CameraInputEventHandler* handler )
    {
        m_intf->unregisterEventHandler( handler );
    }

    //!See nxcip::CameraRelayIOManager::getLastErrorString
    QString CameraRelayIOManager::getLastErrorString() const
    {
        m_intf->getLastErrorString( m_textBuf );
        return QString::fromUtf8( m_textBuf );
    }
}
