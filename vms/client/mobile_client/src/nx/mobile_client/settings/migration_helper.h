#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client_core/connection_context_aware.h>

namespace nx {
namespace mobile_client {
namespace settings {

class SessionsMigrationHelper: public QObject, public QnConnectionContextAware
{
    Q_OBJECT

public:
    SessionsMigrationHelper(QObject* parent = nullptr);
    ~SessionsMigrationHelper();

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace settings
} // namespace mobile_client
} // namespace nx
