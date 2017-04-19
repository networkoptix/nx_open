#pragma once

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

public:
    ResourcePtzController(QObject* parent = nullptr);

    QString resourceId() const;
    void setResourceId(const QString& value);

    bool available() const;

signals:
    void resourceIdChanged();
    void availableChanged();

private:
    QString m_resourceId;
};

} // namespace mobile
} // namespace client
} // namespace nx
