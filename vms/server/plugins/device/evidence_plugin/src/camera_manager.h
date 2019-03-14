#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <QtNetwork/QAuthenticator>
#include <QtCore/QMutex>
#include <QtCore/QString>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/helpers/ptr.h>

#include "ptz_manager.h"

class CameraPlugin;
class MediaEncoder;
class RelayIOManager;
class SyncHttpClient;

namespace nxcip
{
    bool operator<( const nxcip::Resolution& left, const nxcip::Resolution& right );
    bool operator>( const nxcip::Resolution& left, const nxcip::Resolution& right );
}

//!Provides access to camera's properties and instanciates other managers (implements \a nxcip::BaseCameraManager)
class CameraManager
:
    public nxcip::BaseCameraManager
{
public:
    CameraManager( const nxcip::CameraInfo& info );
    virtual ~CameraManager();

    //!Implementaion of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

    //!Implementation of nxcip::BaseCameraManager::getEncoderCount
    virtual int getEncoderCount( int* encoderCount ) const override;
    //!Implementation of nxcip::BaseCameraManager::getEncoder
    virtual int getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr ) override;
    //!Implementation of nxcip::BaseCameraManager::getCameraInfo
    virtual int getCameraInfo( nxcip::CameraInfo* info ) const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
    virtual int getCameraCapabilities( unsigned int* capabilitiesMask ) const override;
    //!Implementation of nxcip::BaseCameraManager::setCredentials
    virtual void setCredentials( const char* username, const char* password ) override;
    //!Implementation of nxcip::BaseCameraManager::setAudioEnabled
    virtual int setAudioEnabled( int audioEnabled ) override;
    //!Implementation of nxcip::BaseCameraManager::getPTZManager
    virtual nxcip::CameraPtzManager* getPtzManager() const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
    virtual nxcip::CameraMotionDataProvider* getCameraMotionDataProvider() const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
    virtual nxcip::CameraRelayIOManager* getCameraRelayIOManager() const override;
    //!Implementation of nxcip::BaseCameraManager::getLastErrorString
    virtual void getLastErrorString( char* errorString ) const override;

    const nxcip::CameraInfo& cameraInfo() const;
    int apiPort() const;
    nxcip::CameraInfo& cameraInfo();
    const QAuthenticator& credentials() const;
    /*!
        \return Error code
    */
    int hiStreamResolutions( const std::vector<nxcip::ResolutionInfo>*& resList ) const;
    /*!
        \return Error code
    */
    int loStreamResolutions( const std::vector<nxcip::ResolutionInfo>*& resList ) const;
    /*!
        \return Error code
    */
    int hiStreamMaxBitrateKbps( int& bitrate ) const;
    /*!
        \return Error code
    */
    int loStreamMaxBitrateKbps( int& bitrate ) const;
    /*!
        \return Error code
    */
    int getRtspPort( int& rtspPort ) const;

    bool isAudioEnabled() const;

    //!reads axis parameter, triggering url like http://ip/axis-cgi/param.cgi?action=list&group=Input.NbrOfInputs
    static int fetchCameraParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        QVariant* paramValue );
    static int fetchCameraParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        QByteArray* paramValue );
    static int fetchCameraParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        QString* paramValue );
    static int fetchCameraParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        unsigned int* paramValue );
    static int fetchCameraParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        int* paramValue );
    static int updateCameraParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        const QByteArray& paramValue );
    static int doCameraRequest(
        SyncHttpClient* const httpClient,
        const QByteArray& requestStr,
        std::multimap<QByteArray, QByteArray>* const responseParams = nullptr );

    int setResolution( int encoderNum, const nxcip::Resolution& resolution );

    nxpt::CommonRefManager* refManager();

private:
    struct ResolutionPair
    {
        nxcip::Resolution hiRes;
        nxcip::Resolution loRes;

        ResolutionPair();
        ResolutionPair(
            nxcip::Resolution _hiRes,
            nxcip::Resolution _loRes );
        bool operator<( const ResolutionPair& right ) const;
        bool operator>( const ResolutionPair& right ) const;
    };

    nxpt::CommonRefManager m_refManager;
    /*!
        Holding reference to \a CameraPlugin, but not \a CameraDiscoveryManager,
        since \a CameraDiscoveryManager instance is not required for \a CameraManager object
    */
    nx::sdk::Ptr<CameraPlugin> m_pluginRef;
    mutable nxcip::CameraInfo m_info;
    const QString m_managementURL;
    QAuthenticator m_credentials;
    mutable std::vector<MediaEncoder*> m_encoders;
    bool m_audioEnabled;
    mutable bool m_relayIOInfoRead;
    mutable std::unique_ptr<RelayIOManager> m_relayIOManager;
    mutable std::unique_ptr<PtzManager> m_ptzManager;
    mutable unsigned int m_cameraCapabilities;
    //!set<pair<hi_stream_res, lo_stream_res> >
    std::set<ResolutionPair, std::greater<ResolutionPair> > m_supportedResolutions;
    std::vector<nxcip::ResolutionInfo> m_hiStreamResolutions;
    std::vector<nxcip::ResolutionInfo> m_loStreamResolutions;
    int m_hiStreamMaxBitrateKbps;
    int m_loStreamMaxBitrateKbps;
    bool m_cameraOptionsRead;
    mutable QMutex m_mutex;
    bool m_forceSecondaryStream;
    int m_rtspPort;
    QStringList m_currentResolutionCoded;
    std::vector<QByteArray> m_relayParamsStr;
    QList<QByteArray> m_cameraParamList;
    QList<QByteArray> m_paramOptions;

    int readCameraOptions();
    void parseResolutionList( const QByteArray& resListStr, int hiStreamMaxFps, int loStreamMaxFps );
    void parseResolutionStr( const QByteArray& resStr, nxcip::Resolution* const resolution );
    QByteArray getResolutionCode( const nxcip::Resolution& resolution );
    template<class ParamAssignFunc>
        int readCameraParam( ParamAssignFunc func ) const;
};
