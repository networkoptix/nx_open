#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnUserWatcher: public Connective<QObject>
{
    Q_OBJECT
    /* This property should remain read-only for QML! */
    Q_PROPERTY(QString userName READ userName NOTIFY userNameChanged)

    using base_type = Connective<QObject>;

public:
    QnUserWatcher(QObject* parent = nullptr);

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
