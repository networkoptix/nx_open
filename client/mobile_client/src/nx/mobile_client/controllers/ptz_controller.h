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

public:
    ResourcePtzController(QObject* parent = nullptr);

    QString resourceId() const;
    void setResourceId(const QString& value);

signals:
    void resourceIdChanged();

private:
    QString m_resourceId;
};

} // namespace mobile
} // namespace client
} // namespace nx
