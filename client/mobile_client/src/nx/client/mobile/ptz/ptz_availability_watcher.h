#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnUuid;

namespace nx {
namespace client {
namespace mobile {

class PtzAvailabilityWatcher: public Connective<QObject>
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
    QnVirtualCameraResourcePtr m_camera;
    bool m_available = false;
};

} // namespace mobile
} // namespace client
} // namespace nx
