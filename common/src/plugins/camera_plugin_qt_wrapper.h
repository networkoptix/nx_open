/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef CAMERA_PLUGIN_QT_WRAPPER_H
#define CAMERA_PLUGIN_QT_WRAPPER_H

#include <QHostAddress>
#include <QList>
#include <QVector>
#include <QString>

#include "camera_plugin.h"


//!Contains wrappers for convinient use of \a nxcip classes
/*!
    All these classes DO NOT increment ref counter at instanciation and DO decrement at destruction
*/
namespace nxcip_qt
{
    class CommonInterfaceRefManager
    {
    public:
        //!Do not calls \a intf->addRef. It MUST be called prior
        CommonInterfaceRefManager( nxpl::NXPluginInterface* const intf );
        //!Calls \a addRef
        CommonInterfaceRefManager( const CommonInterfaceRefManager& );
        virtual ~CommonInterfaceRefManager();

    private:
        nxpl::NXPluginInterface* const m_intf;

        CommonInterfaceRefManager& operator=( const CommonInterfaceRefManager& );
    };

    //!Wrapper for \a nxcip::CameraDiscoveryManager
    class CameraDiscoveryManager
    :
        public CommonInterfaceRefManager
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
        int checkHostAddress( QVector<nxcip::CameraInfo>* const cameras, const QString& url, const QString& login, const QString& password );

        //!See nxcip::CameraDiscoveryManager::fromMDNSData
        int fromMDNSData( const QByteArray& mdnsResponsePacket, const QHostAddress& discoveredAddress, nxcip::CameraInfo* cameraInfo );

        //!See nxcip::CameraDiscoveryManager::fromUpnpData
        bool fromUpnpData( const QByteArray& upnpXMLData, nxcip::CameraInfo* cameraInfo );

        //!See nxcip::CameraDiscoveryManager::createCameraManager
        nxcip::BaseCameraManager* createCameraManager( const nxcip::CameraInfo& info );

    private:
        nxcip::CameraDiscoveryManager* const m_intf;
        char* m_texBuf;
    };

    class CameraMediaEncoder
    :
        public CommonInterfaceRefManager
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
        nxcip::CameraMediaEncoder* const m_intf;
        char* m_textBuf;

        CameraMediaEncoder( const CameraMediaEncoder& );
    };

    //!Wrapper for \a nxcip::BaseCameraManager
    class BaseCameraManager
    :
        public CommonInterfaceRefManager
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

        //!See nxcip::BaseCameraManager::getPTZManager
        nxcip::CameraPTZManager* getPTZManager() const;
        //!See nxcip::BaseCameraManager::getCameraMotionDataProvider
        nxcip::CameraMotionDataProvider* getCameraMotionDataProvider() const;
        //!See nxcip::BaseCameraManager::getCameraRelayIOManager
        nxcip::CameraRelayIOManager* getCameraRelayIOManager() const;

        //!See nxcip::BaseCameraManager::getErrorString
        QString getErrorString( int errorCode ) const;

    private:
        nxcip::BaseCameraManager* const m_intf;
        int m_prevErrorCode;
        char* m_textBuf;

        BaseCameraManager( const BaseCameraManager& );
    };
}

#endif  //CAMERA_PLUGIN_QT_WRAPPER_H
