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

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

    Q_PROPERTY(Ptz::Capabilities capabilities READ capabilities NOTIFY capabilitiesChanged)

public:
    ResourcePtzController(QObject* parent = nullptr);

public: // Properties section
    QString resourceId() const;
    void setResourceId(const QString& value);

    bool available() const;

    Ptz::Capabilities capabilities() const;

public:
    Q_INVOKABLE bool setAutoFocus();

signals:
    void resourceIdChanged();
    void availableChanged();

    void capabilitiesChanged();

private:
    QString m_resourceId;
};

} // namespace mobile
} // namespace client
} // namespace nx
