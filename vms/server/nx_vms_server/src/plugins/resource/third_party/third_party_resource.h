#pragma once

#ifdef ENABLE_THIRD_PARTY

#include <memory>

#include <QCoreApplication>
#include <QSharedPointer>

#include <nx/vms/server/resource/camera_advanced_parameters_providers.h>
#include <nx/vms/server/resource/camera.h>
#include <plugins/camera_plugin_qt_wrapper.h>

//!Camera resource, integrated via external driver. Routes requests to \a nxcip::BaseCameraManager interface
class QnThirdPartyResource
:
    public nx::vms::server::resource::Camera,
    public nxcip::CameraInputEventHandler
{
    Q_DECLARE_TR_FUNCTIONS(QnThirdPartyResource)

    typedef nx::vms::server::resource::Camera base_type;

public:
    static const QString AUX_DATA_PARAM_NAME;

    QnThirdPartyResource(
        QnMediaServerModule* serverModule,
        const nxcip::CameraInfo& camInfo,
        nxcip::BaseCameraManager* camManager,
        const nxcip_qt::CameraDiscoveryManager& discoveryManager );
    virtual ~QnThirdPartyResource();

    //!Implementation of QnNetworkResource::ping
    /*!
        At the moment always returns \a true
    */
    virtual bool ping() override;
    //!Implementation of QnNetworkResource::mergeResourcesIfNeeded
    virtual bool mergeResourcesIfNeeded( const QnNetworkResourcePtr& source ) override;
    //!Implementation of QnSecurityCamResource::manufacturer
    virtual QString getDriverName() const override;
    //!Implementation of QnSecurityCamResource::createLiveDataProvider
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    //!Implementation of QnSecurityCamResource::setOutputPortState
    virtual bool setOutputPortState( const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS ) override;
    //!Implementation of QnSecurityCamResource::createArchiveDataProvider
    virtual QnAbstractStreamDataProvider* createArchiveDataProvider() override;
    //!Implementation of QnSecurityCamResource::createArchiveDelegate
    virtual QnAbstractArchiveDelegate* createArchiveDelegate() override;
    //!Implementation of QnSecurityCamResource::getDtsTimePeriodsByMotionRegion
    virtual QnTimePeriodList getDtsTimePeriodsByMotionRegion(
        const QList<QRegion>& regions,
        qint64 msStartTime,
        qint64 msEndTime,
        int detailLevel,
        bool keepSmalChunks,
        int limit,
        Qt::SortOrder sortOrder) override;

    //!Implementation of QnNetworkResource::getDtsTimePeriods
    virtual QnTimePeriodList getDtsTimePeriods(
        qint64 startTimeMs,
        qint64 endTimeMs,
        int detailLevel,
        bool keepSmalChunks,
        int limit,
        Qt::SortOrder sortOrder) override;

    //!Implementation of nxpl::NXPluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::NXPluginInterface::queryInterface
    virtual int addRef() const override;
    //!Implementation of nxpl::NXPluginInterface::queryInterface
    virtual int releaseRef() const override;

    //!Implementation of nxcip::CameraInputEventHandler::inputPortStateChanged
    virtual void inputPortStateChanged(
        nxcip::CameraRelayIOManager* source,
        const char* inputPortID,
        int newState,
        unsigned long int timestamp ) override;

    const QList<nxcip::Resolution>& getEncoderResolutionList(MotionStreamType encoderNumber) const;

    nxcip::Resolution getSelectedResolutionForEncoder(MotionStreamType encoderIndex ) const;

    QnCameraAdvancedParamValueMap getApiParameters(const QSet<QString>& ids);
    QSet<QString> setApiParameters(const QnCameraAdvancedParamValueMap& values);

protected:
    virtual QnAbstractPtzController* createPtzControllerInternal() const override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

    virtual void setMotionMaskPhysical( int channel );

    virtual std::vector<Camera::AdvancedParametersProvider*> advancedParametersProviders() override;

private:
    struct EncoderData
    {
        QList<nxcip::Resolution> resolutionList;
    };

    // TODO: Migrate to nx::sdk::Ptr.
    nxcip::CameraInfo m_camInfo;
    std::unique_ptr<nxcip_qt::BaseCameraManager> m_camManager;
    nxcip_qt::CameraDiscoveryManager m_discoveryManager;
    QVector<EncoderData> m_encoderData;
    std::unique_ptr<nxcip_qt::CameraRelayIOManager> m_relayIOManager;
    mutable QAtomicInt m_refCounter;
    QString m_defaultOutputID;
    int m_encoderCount;
    std::vector<nxcip::Resolution> m_selectedEncoderResolutions;
    nxcip::BaseCameraManager3* m_cameraManager3;
    nx::vms::server::resource::ApiMultiAdvancedParametersProvider<QnThirdPartyResource> m_advancedParametersProvider;

    bool initializeIOPorts();
    nxcip::Resolution getMaxResolution(MotionStreamType encoderNumber) const;
    //!Returns resolution with pixel count equal or less than \a desiredResolution
    nxcip::Resolution getNearestResolution(MotionStreamType encoderNumber, 
		const nxcip::Resolution& desiredResolution ) const;
    nxcip::Resolution getSecondStreamResolution() const;
    bool setParam(const char * id, const char * value);
};

#endif // ENABLE_THIRD_PARTY
