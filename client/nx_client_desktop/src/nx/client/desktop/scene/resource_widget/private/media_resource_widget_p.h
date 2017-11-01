#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/camera/camera_fwd.h>

#include <utils/common/connective.h>

class QnSingleCamLicenseStatusHelper;

namespace nx {
namespace client {
namespace desktop {

class MediaResourceWidgetPrivate: public Connective<QObject>
{
    Q_OBJECT
    Q_PROPERTY(bool hasVideo MEMBER hasVideo CONSTANT);
    Q_PROPERTY(bool isIoModule MEMBER isIoModule CONSTANT);
    Q_PROPERTY(bool isPlayingLive READ isPlayingLive WRITE setIsPlayingLive NOTIFY stateChanged);
    Q_PROPERTY(bool isOffline READ isOffline WRITE setIsOffline NOTIFY stateChanged);
    Q_PROPERTY(bool isUnauthorized READ isUnauthorized WRITE setIsUnauthorized NOTIFY stateChanged);
    Q_PROPERTY(bool nvrWithoutLicense READ nvrWithoutLicense WRITE setNvrWithoutLicense NOTIFY stateChanged);

    using base_type = Connective<QObject>;

public:
    const QnResourcePtr resource;
    const QnMediaResourcePtr mediaResource;
    const QnVirtualCameraResourcePtr camera;
    const bool hasVideo;
    const bool isIoModule;

public:
    explicit MediaResourceWidgetPrivate(const QnResourcePtr& resource, QObject* parent = nullptr);

    QnResourceDisplayPtr display() const;
    void setDisplay(const QnResourceDisplayPtr& display);

    bool isPlayingLive() const;
    bool isOffline() const;
    bool isUnauthorized() const;
    bool nvrWithoutLicense() const;

signals:
    void stateChanged();

private:
    void updateIsPlayingLive();
    void setIsPlayingLive(bool value);

    void updateIsOffline();
    void setIsOffline(bool value);

    void updateIsUnauthorized();
    void setIsUnauthorized(bool value);

    void updateNvrWithoutLicense();
    void setNvrWithoutLicense(bool value);

private:
    QnResourceDisplayPtr m_display;
    QScopedPointer<QnSingleCamLicenseStatusHelper> m_licenseStatusHelper;

    bool m_isPlayingLive = false;
    bool m_isOffline = false;
    bool m_isUnauthorized = false;
    bool m_nvrWithoutLicense = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
