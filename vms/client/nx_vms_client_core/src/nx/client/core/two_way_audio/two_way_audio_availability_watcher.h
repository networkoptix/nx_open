#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <client_core/connection_context_aware.h>

class QnUuid;
class QnSingleCamLicenseStatusHelper;

namespace nx::vms::client::core {

class TwoWayAudioAvailabilityWatcher: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

public:
    TwoWayAudioAvailabilityWatcher(QObject* parent = nullptr);

    virtual ~TwoWayAudioAvailabilityWatcher() override;

    void setResourceId(const QnUuid& uuid);

    bool available() const;

signals:
    void availabilityChanged();

private:
    void updateAvailability();

    void setAvailable(bool value);

private:
    bool m_available = false;
    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<QnSingleCamLicenseStatusHelper> m_licenseHelper;
};

} // namespace nx::vms::client::core
