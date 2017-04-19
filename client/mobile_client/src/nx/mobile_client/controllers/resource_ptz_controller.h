#pragma once

#include <core/ptz/ptz_constants.h>
#include <core/ptz/proxy_ptz_controller.h>

namespace nx {
namespace client {
namespace mobile {

class ResourcePtzController: public QnProxyPtzController
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

    Q_PROPERTY(QUuid uniqueResourceId READ uniqueResourceId
        WRITE setUniqueResourceId NOTIFY uniqueResourceIdChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

    Q_PROPERTY(Ptz::Capabilities capabilities READ capabilities NOTIFY capabilitiesChanged)

public:
    ResourcePtzController(QObject* parent = nullptr);

public: // Properties section
    QUuid uniqueResourceId() const;
    void setUniqueResourceId(const QUuid& value);

    bool available() const;

    Ptz::Capabilities capabilities() const;

public:
    Q_INVOKABLE bool setAutoFocus();

signals:
    void uniqueResourceIdChanged();
    void availableChanged();

    void capabilitiesChanged();

private:
    QUuid m_uniqueResourceId;
};

} // namespace mobile
} // namespace client
} // namespace nx
