/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_RESOURCE_H
#define THIRD_PARTY_RESOURCE_H

#include <QSharedPointer>

#include "core/resource/camera_resource.h"
#include "../../camera_plugin_qt_wrapper.h"


//!Camera resource, integrated via external driver. Routes requests to \a nxcip::BaseCameraManager interface
class QnThirdPartyResource
:
    public QnPhysicalCameraResource
{
public:
    QnThirdPartyResource(
        const nxcip::CameraInfo& camInfo,
        nxcip::BaseCameraManager* camManager,
        const nxcip_qt::CameraDiscoveryManager& discoveryManager );
    virtual ~QnThirdPartyResource();

    //!Implementation of QnResource::getPtzController
    QnAbstractPtzController* getPtzController();
    //!Implementation of QnNetworkResource::isResourceAccessible
    virtual bool isResourceAccessible() override;
    //!Implementation of QnSecurityCamResource::manufacture
    virtual QString manufacture() const override;
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
    virtual bool setRelayOutputState( const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS );

    const QList<nxcip::Resolution>& getEncoderResolutionList( int encoderNumber ) const;

signals:
    //!Emitted on camera input port state has been changed
    /*!
        \param resource Smart pointer to \a this
        \param inputPortID
        \param value true if input is connected, false otherwise
        \param timestamp MSecs since epoch, UTC
    */
    void cameraInput(
        QnResourcePtr resource,
        const QString& inputPortID,
        bool value,
        qint64 timestamp);

protected:
    //!Implementation of QnResource::initInternal
    virtual bool initInternal() override;
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
};

typedef QSharedPointer<QnThirdPartyResource> QnThirdPartyResourcePtr;

#endif  //THIRD_PARTY_RESOURCE_H
