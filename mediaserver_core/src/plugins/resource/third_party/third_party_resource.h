#pragma once

#ifdef ENABLE_THIRD_PARTY

#include <memory>

#include <QCoreApplication>
#include <QSharedPointer>

#include <nx/mediaserver/resource/camera.h>
#include <plugins/camera_plugin_qt_wrapper.h>

//!Camera resource, integrated via external driver. Routes requests to \a nxcip::BaseCameraManager interface
class QnThirdPartyResource
:
    public nx::mediaserver::resource::Camera,
    public nxcip::CameraInputEventHandler
{
    Q_DECLARE_TR_FUNCTIONS(QnThirdPartyResource)

    typedef nx::mediaserver::resource::Camera base_type;

public:
    static const QString AUX_DATA_PARAM_NAME;

    QnThirdPartyResource(
        const nxcip::CameraInfo& camInfo,
        nxcip::BaseCameraManager* camManager,
        const nxcip_qt::CameraDiscoveryManager& discoveryManager );
    virtual ~QnThirdPartyResource();

    //!Implementation of QnResource::getPtzController
    virtual QnAbstractPtzController* createPtzControllerInternal() override;
    
    //!Implementation of QnResource::getParamPhysical
    virtual bool getParamPhysical(const QString &id, QString &value) override;
    //!Implementation of QnResource::setParamPhysical
    virtual bool setParamPhysical(const QString &id, const QString &value) override;
    virtual bool setParamsBegin() override;
    virtual bool setParamsEnd() override;

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
    virtual QnIOPortDataList getRelayOutputList() const override;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;

    //!Implementation of QnSecurityCamResource::getInputPortList
    virtual QnIOPortDataList getInputPortList() const override;
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
    std::unique_ptr<nxcip_qt::CameraRelayIOManager> m_relayIOManager;
    QAtomicInt m_refCounter;
    QString m_defaultOutputID;
    int m_encoderCount;
    std::vector<nxcip::Resolution> m_selectedEncoderResolutions;
    nxcip::BaseCameraManager3* m_cameraManager3;

    bool initializeIOPorts();
    nxcip::Resolution getMaxResolution( int encoderNumber ) const;
    //!Returns resolution with pixel count equal or less than \a desiredResolution
    nxcip::Resolution getNearestResolution( int encoderNumber, const nxcip::Resolution& desiredResolution ) const;
    nxcip::Resolution getSecondStreamResolution() const;
    bool setParam(const char * id, const char * value);
};

#endif // ENABLE_THIRD_PARTY
