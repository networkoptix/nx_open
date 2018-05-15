#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace core {

class UserWatcher: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT
    /* This property should remain read-only for QML! */
    Q_PROPERTY(QString userName READ userName NOTIFY userNameChanged)

    using base_type = Connective<QObject>;

public:
    UserWatcher(QObject* parent = nullptr);

    void setUserName(const QString& name);
    const QString& userName() const;

    const QnUserResourcePtr& user() const;

signals:
    void userChanged(const QnUserResourcePtr& user);
    void userNameChanged();

private:
    void setCurrentUser(const QnUserResourcePtr& currentUser);

private:
    QString m_userName;
    QnUserResourcePtr m_user;
};

} // namespace core
} // namespace client
} // namespace nx
