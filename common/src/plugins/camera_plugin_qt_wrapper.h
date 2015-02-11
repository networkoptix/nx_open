/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef CAMERA_PLUGIN_QT_WRAPPER_H
#define CAMERA_PLUGIN_QT_WRAPPER_H

#include <QtNetwork/QHostAddress>
#include <QtCore/QList>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <utils/common/mutex.h>

#include "camera_plugin.h"


//!Contains wrappers for convenient use of \a nxcip classes
/*!
    All these classes DO NOT increment ref counter at instantiation and DO decrement at destruction
*/
namespace nxcip_qt
{
    template<class InterfaceType>
    class CommonInterfaceRefManager
    {
    public:
        //!Do not calls \a intf->addRef. It MUST be called prior
        CommonInterfaceRefManager( InterfaceType* const intf )
        :
            m_intf( intf )
        {
        }

        //!Calls \a addRef
        CommonInterfaceRefManager( const CommonInterfaceRefManager& origin )
        :
            m_intf( origin.m_intf )
        {
            m_intf->addRef();
        }

        virtual ~CommonInterfaceRefManager()
        {
            m_intf->releaseRef();
        }

        CommonInterfaceRefManager& operator=( const CommonInterfaceRefManager& right )
        {
            m_intf->releaseRef();
            m_intf = right.m_intf;
            m_intf->addRef();
            return *this;
        }

    protected:
        InterfaceType* m_intf;
    };

    //!Wrapper for \a nxcip::CameraDiscoveryManager
    class CameraDiscoveryManager
    :
        public CommonInterfaceRefManager<nxcip::CameraDiscoveryManager>
    {
    public:
        CameraDiscoveryManager( nxcip::CameraDiscoveryManager* const intf );
        CameraDiscoveryManager( const CameraDiscoveryManager& );
        ~CameraDiscoveryManager();

        nxcip::CameraDiscoveryManager* getRef() { return m_intf; }

        //!See nxcip::CameraDiscoveryManager::getVendorName
        QString getVendorName() const;
        //!See nxcip::CameraDiscoveryManager::findCameras
        int findCameras( QVector<nxcip::CameraInfo>* const cameras, const QString& localInterfaceIPAddr );
        //!See nxcip::CameraDiscoveryManager::checkHostAddress
        int checkHostAddress( QVector<nxcip::CameraInfo>* const cameras, const QString& url, const QString* login, const QString* password );
        //!See nxcip::CameraDiscoveryManager::fromMDNSData
        int fromMDNSData( const QByteArray& mdnsResponsePacket, const QHostAddress& discoveredAddress, nxcip::CameraInfo* cameraInfo );
        //!See nxcip::CameraDiscoveryManager::fromUpnpData
        bool fromUpnpData( const QByteArray& upnpXMLData, nxcip::CameraInfo* cameraInfo );
        //!See nxcip::CameraDiscoveryManager::createCameraManager
        nxcip::BaseCameraManager* createCameraManager( const nxcip::CameraInfo& info );
        //!See \a nxcip::CameraDiscoveryManager::getReservedModelListFirst and \a nxcip::CameraDiscoveryManager::getReservedModelListNext
        QList<QString> getReservedModelList() const;

        const CameraDiscoveryManager& operator=(const CameraDiscoveryManager& right);

    private:
        mutable QnMutex m_mutex;
        char* m_texBuf;
    };

    class CameraMediaEncoder
    :
        public CommonInterfaceRefManager<nxcip::CameraMediaEncoder>
    {
    public:
        CameraMediaEncoder( nxcip::CameraMediaEncoder* const intf );
        ~CameraMediaEncoder();

        nxcip::CameraMediaEncoder* getRef() { return m_intf; }

        //!See nxcip::BaseCameraManager::getMediaUrl
        int getMediaUrl( QString* const url ) const;
        //!See nxcip::BaseCameraManager::getResolutionList
        int getResolutionList( QVector<nxcip::ResolutionInfo>* infoList ) const;
        //!See nxcip::BaseCameraManager::getMaxBitrate
        int getMaxBitrate( int* maxBitrate ) const;
        //!See nxcip::BaseCameraManager::setResolution
        int setResolution( const nxcip::Resolution& resolution );
        //!See nxcip::BaseCameraManager::setFps
        int setFps( const float& fps, float* selectedFps );
        //!See nxcip::BaseCameraManager::setBitrate
        int setBitrate( int bitrateKbps, int* selectedBitrateKbps );

    private:
        char* m_textBuf;

        CameraMediaEncoder( const CameraMediaEncoder& );
    };

    //!Wrapper for \a nxcip::BaseCameraManager
    /*!
        \note All methods are thread-safe
    */
    class BaseCameraManager
    :
        public CommonInterfaceRefManager<nxcip::BaseCameraManager>
    {
    public:
        BaseCameraManager( nxcip::BaseCameraManager* const intf );
        ~BaseCameraManager();

        nxcip::BaseCameraManager* getRef() { return m_intf; }

        //!See nxcip::BaseCameraManager::getEncoderCount
        int getEncoderCount( int* encoderCount ) const;
        //!See nxcip::BaseCameraManager::getEncoder
        int getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr );
        //!See nxcip::BaseCameraManager::getCameraInfo
        int getCameraInfo( nxcip::CameraInfo* info ) const;
        //!See nxcip::BaseCameraManager::getCameraCapabilities
        int getCameraCapabilities( unsigned int* capabilitiesMask ) const;
        //!See nxcip::BaseCameraManager::setCredentials
        void setCredentials( const QString& username, const QString& password );
        //!See nxcip::BaseCameraManager::setAudioEnabled
        int setAudioEnabled( bool audioEnabled );

        //!See nxcip::BaseCameraManager::getPtzManager
        nxcip::CameraPtzManager* getPtzManager() const;
        //!See nxcip::BaseCameraManager::getCameraRelayIOManager
        nxcip::CameraRelayIOManager* getCameraRelayIOManager() const;

        //!See nxcip::BaseCameraManager::getLastErrorString
        QString getLastErrorString() const;

    private:
        mutable QnMutex m_mutex;
        int m_prevErrorCode;
        char* m_textBuf;

        BaseCameraManager( const BaseCameraManager& );
    };

    //!Wrapper for \a nxcip::CameraRelayIOManager
    class CameraRelayIOManager
    :
        public CommonInterfaceRefManager<nxcip::CameraRelayIOManager>
    {
    public:
        CameraRelayIOManager( nxcip::CameraRelayIOManager* const intf );
        ~CameraRelayIOManager();

        nxcip::CameraRelayIOManager* getRef() { return m_intf; }

        //!See nxcip::CameraRelayIOManager::getRelayOutputList
        int getRelayOutputList( QStringList* const ids ) const;
        //!See nxcip::CameraRelayIOManager::getInputPortList
        int getInputPortList( QStringList* const ids ) const;
        //!See nxcip::CameraRelayIOManager::setRelayOutputState
        int setRelayOutputState(
            const QString& outputID,
            bool activate,
            unsigned int autoResetTimeoutMS );
        //!See nxcip::CameraRelayIOManager::startInputPortMonitoring
        int startInputPortMonitoring();
        //!See nxcip::CameraRelayIOManager::stopInputPortMonitoring
        void stopInputPortMonitoring();
        //!See nxcip::CameraRelayIOManager::registerEventHandler
        void registerEventHandler( nxcip::CameraInputEventHandler* handler );
        //!See nxcip::CameraRelayIOManager::unregisterEventHandler
        void unregisterEventHandler( nxcip::CameraInputEventHandler* handler );

        //!See nxcip::CameraRelayIOManager::getLastErrorString
        QString getLastErrorString() const;

    private:
        char** m_idsList;
        char* m_textBuf;

        CameraRelayIOManager( const CameraRelayIOManager& );
    };
}

#endif  //CAMERA_PLUGIN_QT_WRAPPER_H
