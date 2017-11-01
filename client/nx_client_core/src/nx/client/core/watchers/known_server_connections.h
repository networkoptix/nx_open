#pragma once

#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <nx/fusion/model_functions_fwd.h>

class QTimer;

class QnCommonModule;

namespace nx {
namespace client {
namespace core {
namespace watchers {

class KnownServerConnections: public QObject
{
    Q_OBJECT

public:
    struct Connection
    {
        QnUuid serverId;
        nx::utils::Url url;
    };

    explicit KnownServerConnections(QnCommonModule* commonModule, QObject* parent = nullptr);
    ~KnownServerConnections();

    void start();

private:
    class Private;
    QScopedPointer<Private> const d;
};

QN_FUSION_DECLARE_FUNCTIONS(KnownServerConnections::Connection, (json)(eq))

} // namespace watchers
} // namespace core
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::core::watchers::KnownServerConnections::Connection)
