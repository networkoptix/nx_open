#pragma once

#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>
#include <common/common_module_aware.h>

namespace nx {
namespace client {
namespace core {
namespace watchers {

class KnownServerConnections: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    struct Connection
    {
        QnUuid id;
        QUrl url;
    };

    explicit KnownServerConnections(QnCommonModule* commonModule, QObject* parent = nullptr);

private:
    QList<Connection> m_connections;
};

QN_FUSION_DECLARE_FUNCTIONS(KnownServerConnections::Connection, (json)(eq))

} // namespace watchers
} // namespace core
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::core::watchers::KnownServerConnections::Connection)
