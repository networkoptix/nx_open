#pragma once

#include <resources/resource_helper.h>
#include <core/ptz/media_dewarping_params.h>

class QnMediaResourceHelperPrivate;
class QnMediaResourceHelper: public QnResourceHelper
{
    Q_OBJECT

    Q_PROPERTY(QString serverName READ serverName NOTIFY serverNameChanged)
    Q_PROPERTY(qreal customAspectRatio READ customAspectRatio NOTIFY customAspectRatioChanged)
    Q_PROPERTY(int customRotation READ customRotation NOTIFY customRotationChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY videoLayoutChanged)
    Q_PROPERTY(QSize layoutSize READ layoutSize NOTIFY videoLayoutChanged)
    Q_PROPERTY(QnMediaDewarpingParams fisheyeParams READ fisheyeParams NOTIFY fisheyeParamsChanged)
    Q_PROPERTY(bool analogCameraWithoutLicense READ analogCameraWithoutLicense
        NOTIFY analogCameraWithoutLicenseChanged)
    Q_PROPERTY(bool isWearableCamera READ isWearableCamera NOTIFY wearableCameraChanged)

    Q_ENUMS(Qn::ResourceStatus)

    using base_type = QnResourceHelper;

public:
    explicit QnMediaResourceHelper(QObject* parent = nullptr);
    virtual ~QnMediaResourceHelper();

    QString serverName() const;
    qreal customAspectRatio() const;
    int customRotation() const;
    int channelCount() const;
    QSize layoutSize() const;
    QnMediaDewarpingParams fisheyeParams() const;
    bool analogCameraWithoutLicense() const;
    Q_INVOKABLE QPoint channelPosition(int channel) const;
    bool isWearableCamera() const;

signals:
    void serverNameChanged();
    void customAspectRatioChanged();
    void customRotationChanged();
    void videoLayoutChanged();
    void fisheyeParamsChanged();
    void analogCameraWithoutLicenseChanged();
    void wearableCameraChanged();

private:
    QScopedPointer<QnMediaResourceHelperPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMediaResourceHelper)
};
