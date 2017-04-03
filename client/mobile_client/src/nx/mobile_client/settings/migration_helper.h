#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client_core/connection_context_aware.h>

namespace nx {
namespace mobile_client {
namespace settings {

class SessionsMigrationHelperPrivate;
class SessionsMigrationHelper: public QObject, public QnConnectionContextAware
{
    Q_OBJECT

public:
    SessionsMigrationHelper(QObject* parent = nullptr);
    ~SessionsMigrationHelper();

private:
    QScopedPointer<SessionsMigrationHelperPrivate> d_ptr;
    Q_DECLARE_PRIVATE(SessionsMigrationHelper)
};

} // namespace settings
} // namespace mobile_client
} // namespace nx
