#pragma once

#include <QtCore/QObject>

class QnUuid;

namespace nx {
namespace client {
namespace mobile {

class PtzAvailabilityWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

public:
    PtzAvailabilityWatcher(QObject* parent = nullptr);

    void setResourceId(const QnUuid& uuid);

    bool available() const;

signals:
    void availabilityChanged();

private:
    void setAvailable(bool value);

private:
    bool m_available = false;
};

} // namespace mobile
} // namespace client
} // namespace nx
