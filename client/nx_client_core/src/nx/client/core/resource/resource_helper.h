#pragma once

#include <common/common_globals.h>
#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <client_core/connection_context_aware.h>

namespace nx {
namespace client {
namespace core {

class ResourceHelper: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(Qn::ResourceStatus resourceStatus READ resourceStatus NOTIFY resourceStatusChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)

    Q_ENUMS(Qn::ResourceStatus)

    using base_type = Connective<QObject>;

public:
    ResourceHelper(QObject* parent = nullptr);

    QString resourceId() const;
    void setResourceId(const QString& id);

    Qn::ResourceStatus resourceStatus() const;
    QString resourceName() const;

protected:
    QnResourcePtr resource() const;

signals:
    void resourceIdChanged();
    void resourceStatusChanged();
    void resourceNameChanged();

private:
    QnResourcePtr m_resource;
};

} // namespace core
} // namespace client
} // namespace nx
