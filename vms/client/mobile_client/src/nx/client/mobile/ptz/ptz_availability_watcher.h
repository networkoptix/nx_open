#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <client_core/connection_context_aware.h>
#include <core/resource/client_core_camera.h>

class QnUuid;

namespace nx::vms::client::core { class Camera; }

namespace nx {
namespace client {
namespace mobile {

class PtzAvailabilityWatcher: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

public:
    PtzAvailabilityWatcher(QObject* parent = nullptr);

    void setResourceId(const QnUuid& uuid);

    bool available() const;

signals:
    void availabilityChanged();

private:
    void setAvailable(bool value);

    void updateAvailability();

private:
    nx::vms::client::core::CameraPtr m_camera;
    bool m_available = false;
};

} // namespace mobile
} // namespace client
} // namespace nx
