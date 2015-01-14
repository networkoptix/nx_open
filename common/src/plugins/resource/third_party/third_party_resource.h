/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_RESOURCE_H
#define THIRD_PARTY_RESOURCE_H

#ifdef ENABLE_THIRD_PARTY

#include <memory>

#include <QSharedPointer>

#include <plugins/resource/camera_settings/camera_settings.h>

#include "core/resource/camera_resource.h"
#include "../../camera_plugin_qt_wrapper.h"


//!Camera resource, integrated via external driver. Routes requests to \a nxcip::BaseCameraManager interface
class QnThirdPartyResource
:
    public QnPhysicalCameraResource,
    public nxcip::CameraInputEventHandler
{
    typedef QnPhysicalCameraResource base_type;

public:
    static const int PRIMARY_ENCODER_INDEX = 0;
    static const int SECONDARY_ENCODER_INDEX = 1;
    static const QString AUX_DATA_PARAM_NAME;

    QnThirdPartyResource(
        const nxcip::CameraInfo& camInfo,
        nxcip::BaseCameraManager* camManager,
        const nxcip_qt::CameraDiscoveryManager& discoveryManager );
    virtual ~QnThirdPartyResource();

    //!Implementation of QnResource::getPtzController
    virtual QnAbstractPtzController* createPtzControllerInternal() override;
    //!Implementation of QnResource::getParamPhysical
    virtual bool getParamPhysical( const QString& param, QVariant& val ) override;
    //!Implementation of QnResource::setParamPhysical
    virtual bool setParamPhysical( const QString& param, const QVariant& val ) override;

    //!Implementation of QnNetworkResource::ping
    /*!
        At the moment always returns \a true
    */
    virtual bool ping() override;
    //!Implementation of QnNetworkResource::mergeResourcesIfNeeded
    virtual bool mergeResourcesIfNeeded( const QnNetworkResourcePtr& source ) override;
    //!Implementation of QnSecurityCamResource::manufacture
    virtual QString getDriverName() const override;
    //!Implementation of QnSecurityCamResource::setIframeDistance
    virtual void setIframeDistance( int frames, int timems ) override;
    //!Implementation of QnSecurityCamResource::createLiveDataProvider
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getRelayOutputList() const override;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;

    //!Implementation of QnSecurityCamResource::getInputPortList
    virtual QStringList getInputPortList() const override;
    //!Implementation of QnSecurityCamResource::setRelayOutputState
    virtual bool setRelayOutputState( const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS ) override;
    //!Implementation of QnSecurityCamResource::createArchiveDataProvider
    virtual QnAbstractStreamDataProvider* createArchiveDataProvider() override;
    //!Implementation of QnSecurityCamResource::createArchiveDelegate
    virtual QnAbstractArchiveDelegate* createArchiveDelegate() override;
    //!Implementation of QnSecurityCamResource::getDtsTimePeriodsByMotionRegion
    virtual QnTimePeriodList getDtsTimePeriodsByMotionRegion(
        const QList<QRegion>& regions,
        qint64 msStartTime,
        qint64 msEndTime,
        int detailLevel ) override;

    //!Implementation of QnNetworkResource::getDtsTimePeriods
    virtual QnTimePeriodList getDtsTimePeriods( qint64 startTimeMs, qint64 endTimeMs, int detailLevel ) override;

    //!Implementation of nxpl::NXPluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::NXPluginInterface::queryInterface
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::NXPluginInterface::queryInterface
    virtual unsigned int releaseRef() override;

    //!Implementation of nxcip::CameraInputEventHandler::inputPortStateChanged
    virtual void inputPortStateChanged(
        nxcip::CameraRelayIOManager* source,
        const char* inputPortID,
        int newState,
        unsigned long int timestamp ) override;

    const QList<nxcip::Resolution>& getEncoderResolutionList( int encoderNumber ) const;
    virtual bool hasDualStreaming() const override;

    nxcip::Resolution getSelectedResolutionForEncoder( int encoderIndex ) const;

protected:
    //!Implementation of QnResource::initInternal
    virtual CameraDiagnostics::Result initInternal() override;
    //!Implementation of QnSecurityCamResource::startInputPortMonitoringAsync
    virtual bool startInputPortMonitoringAsync( std::function<void(bool)>&& completionHandler ) override;
    //!Implementation of QnSecurityCamResource::stopInputPortMonitoringAsync
    virtual void stopInputPortMonitoringAsync() override;
    //!Implementation of QnSecurityCamResource::isInputPortMonitored
    virtual bool isInputPortMonitored() const override;
    //!Implementation of QnSecurityCamResource::setMotionMaskPhysical
    virtual void setMotionMaskPhysical( int channel );

private:
    struct EncoderData
    {
        QList<nxcip::Resolution> resolutionList;
    };

    nxcip::CameraInfo m_camInfo;
    std::unique_ptr<nxcip_qt::BaseCameraManager> m_camManager;
    nxcip_qt::CameraDiscoveryManager m_discoveryManager;
    QVector<EncoderData> m_encoderData;
    std::auto_ptr<nxcip_qt::CameraRelayIOManager> m_relayIOManager;
    QAtomicInt m_refCounter;
    QString m_defaultOutputID;
    int m_encoderCount;
    std::vector<nxcip::Resolution> m_selectedEncoderResolutions;
    nxcip::BaseCameraManager3* m_cameraManager3;
    std::map<QString, CameraSetting> m_cameraSettings;

    bool initializeIOPorts();
    nxcip::Resolution getMaxResolution( int encoderNumber ) const;
    //!Returns resolution with pixel count equal or less than \a desiredResolution
    nxcip::Resolution getNearestResolution( int encoderNumber, const nxcip::Resolution& desiredResolution ) const;
    nxcip::Resolution getSecondStreamResolution() const;
};

#endif // ENABLE_THIRD_PARTY

#endif  //THIRD_PARTY_RESOURCE_H
