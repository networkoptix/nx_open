#pragma once

#include <QtCore/QSize>
#include <QtCore/QPoint>
#include <QtCore/QString>
#include <QtCore/QScopedPointer>

#include <core/ptz/media_dewarping_params.h>

#include <nx/client/core/resource/resource_helper.h>

namespace nx::vms::client::core {

class MediaResourceHelper: public ResourceHelper
{
    Q_OBJECT

    Q_PROPERTY(QString serverName READ serverName NOTIFY serverNameChanged)
    Q_PROPERTY(qreal customAspectRatio READ customAspectRatio NOTIFY customAspectRatioChanged)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio NOTIFY aspectRatioChanged)
    Q_PROPERTY(int customRotation READ customRotation NOTIFY customRotationChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY videoLayoutChanged)
    Q_PROPERTY(QSize layoutSize READ layoutSize NOTIFY videoLayoutChanged)
    Q_PROPERTY(QnMediaDewarpingParams fisheyeParams READ fisheyeParams NOTIFY fisheyeParamsChanged)
    Q_PROPERTY(bool analogCameraWithoutLicense READ analogCameraWithoutLicense
        NOTIFY analogCameraWithoutLicenseChanged)
    Q_PROPERTY(bool isWearableCamera READ isWearableCamera NOTIFY wearableCameraChanged)

    Q_ENUMS(Qn::ResourceStatus)

    using base_type = ResourceHelper;

public:
    explicit MediaResourceHelper(QObject* parent = nullptr);
    virtual ~MediaResourceHelper();

    QString serverName() const;
    qreal customAspectRatio() const;
    qreal aspectRatio() const; //< Effective aspect ratio, custom (if set) or native.
    int customRotation() const;
    int channelCount() const;
    QSize layoutSize() const;
    QnMediaDewarpingParams fisheyeParams() const;
    bool analogCameraWithoutLicense() const;
    Q_INVOKABLE QPoint channelPosition(int channel) const;
    bool isWearableCamera() const;

signals:
    void serverNameChanged();
    void aspectRatioChanged();
    void customAspectRatioChanged();
    void customRotationChanged();
    void videoLayoutChanged();
    void fisheyeParamsChanged();
    void analogCameraWithoutLicenseChanged();
    void wearableCameraChanged();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::core
