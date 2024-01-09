// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPoint>
#include <QtCore/QScopedPointer>
#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/client/core/media/media_player.h>
#include <nx/vms/client/core/resource/media_dewarping_params.h>
#include <nx/vms/client/core/resource/resource_helper.h>

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
    Q_PROPERTY(nx::vms::client::core::MediaDewarpingParams fisheyeParams
        READ fisheyeParams
        NOTIFY fisheyeParamsChanged)
    Q_PROPERTY(bool analogCameraWithoutLicense READ analogCameraWithoutLicense
        NOTIFY analogCameraWithoutLicenseChanged)
    Q_PROPERTY(bool isVirtualCamera READ isVirtualCamera NOTIFY virtualCameraChanged)
    Q_PROPERTY(bool audioEnabled READ audioEnabled NOTIFY audioEnabledChanged)
    Q_PROPERTY(int livePreviewVideoQuality READ livePreviewVideoQuality
        NOTIFY livePreviewVideoQualityChanged)

    using base_type = ResourceHelper;

public:
    MediaResourceHelper(SystemContext* systemContext, QObject* parent = nullptr);
    MediaResourceHelper(); //< QML constructor.
    virtual ~MediaResourceHelper();

    QString serverName() const;
    qreal customAspectRatio() const;
    qreal aspectRatio() const; //< Effective aspect ratio, custom (if set) or native.
    int customRotation() const;
    int channelCount() const;
    QSize layoutSize() const;
    MediaDewarpingParams fisheyeParams() const;
    bool analogCameraWithoutLicense() const;
    Q_INVOKABLE QPoint channelPosition(int channel) const;
    bool isVirtualCamera() const;
    bool audioEnabled() const;
    MediaPlayer::VideoQuality livePreviewVideoQuality() const;
    Q_INVOKABLE MediaPlayer::VideoQuality streamQuality(
        nx::vms::api::StreamIndex streamIndex) const;

signals:
    void serverNameChanged();
    void aspectRatioChanged();
    void customAspectRatioChanged();
    void customRotationChanged();
    void videoLayoutChanged();
    void fisheyeParamsChanged();
    void analogCameraWithoutLicenseChanged();
    void virtualCameraChanged();
    void audioEnabledChanged();
    void livePreviewVideoQualityChanged();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::core
