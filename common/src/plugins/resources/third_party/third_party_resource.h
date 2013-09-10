/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_RESOURCE_H
#define THIRD_PARTY_RESOURCE_H

#include <memory>

#include <QSharedPointer>

#include "core/resource/camera_resource.h"
#include "../../camera_plugin_qt_wrapper.h"


//!Camera resource, integrated via external driver. Routes requests to \a nxcip::BaseCameraManager interface
class QnThirdPartyResource
:
    public QnPhysicalCameraResource,
    public nxcip::CameraInputEventHandler
{
    Q_OBJECT

public:
    QnThirdPartyResource(
        const nxcip::CameraInfo& camInfo,
        nxcip::BaseCameraManager* camManager,
        const nxcip_qt::CameraDiscoveryManager& discoveryManager );
    virtual ~QnThirdPartyResource();

    //!Implementation of QnSecurityCamResource::createArchiveDataProvider
    virtual QnAbstractStreamDataProvider* createArchiveDataProvider() override;
    //!Implementation of QnSecurityCamResource::createArchiveDelegate
    virtual QnAbstractArchiveDelegate* createArchiveDelegate() override;

    //!Implementation of QnResource::getPtzController
    virtual QnAbstractPtzController* getPtzController() override;
    //!Implementation of QnNetworkResource::isResourceAccessible
    virtual bool isResourceAccessible() override;
    //!Implementation of QnNetworkResource::ping
    /*!
        At the moment always returns \a true
    */
    virtual bool ping() override;
    //!Implementation of QnSecurityCamResource::manufacture
    virtual QString getDriverName() const override;
    //!Implementation of QnSecurityCamResource::setIframeDistance
    virtual void setIframeDistance( int frames, int timems ) override;
    //!Implementation of QnSecurityCamResource::createLiveDataProvider
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    //!Implementation of QnSecurityCamResource::setCropingPhysical
    virtual void setCropingPhysical( QRect croppingRect ) override;
    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getRelayOutputList() const override;
    //!Implementation of QnSecurityCamResource::getInputPortList
    virtual QStringList getInputPortList() const override;
    //!Implementation of QnSecurityCamResource::setRelayOutputState
    virtual bool setRelayOutputState( const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS ) override;

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

protected:
    //!Implementation of QnResource::initInternal
    virtual CameraDiagnostics::Result initInternal() override;
    //!Implementation of QnSecurityCamResource::startInputPortMonitoring
    virtual bool startInputPortMonitoring() override;
    //!Implementation of QnSecurityCamResource::stopInputPortMonitoring
    virtual void stopInputPortMonitoring() override;
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
    nxcip_qt::BaseCameraManager m_camManager;
    nxcip_qt::CameraDiscoveryManager m_discoveryManager;
    QVector<EncoderData> m_encoderData;
    std::auto_ptr<nxcip_qt::CameraRelayIOManager> m_relayIOManager;
    QAtomicInt m_refCounter;
    QString m_defaultOutputID;

    bool initializeIOPorts();
};

typedef QSharedPointer<QnThirdPartyResource> QnThirdPartyResourcePtr;

#endif  //THIRD_PARTY_RESOURCE_H
