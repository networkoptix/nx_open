#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnUuid;
class QnSingleCamLicenseStatusHelper;

namespace nx {
namespace client {
namespace core {

class TwoWayAudioAvailabilityWatcher: public Connective<QObject>
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

} // namespace core
} // namespace client
} // namespace nx
