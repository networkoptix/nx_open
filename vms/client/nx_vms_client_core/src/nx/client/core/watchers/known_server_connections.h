#pragma once

#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <nx/fusion/model_functions_fwd.h>

class QTimer;

class QnCommonModule;

namespace nx::vms::client::core {
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
} // namespace nx::vms::client::core

Q_DECLARE_METATYPE(nx::vms::client::core::watchers::KnownServerConnections::Connection)
