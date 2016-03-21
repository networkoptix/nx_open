#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnUserWatcher: public Connective<QObject> {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnUserWatcher(QObject *parent = NULL);

    virtual ~QnUserWatcher();

    void setUserName(const QString &name);
    const QString &userName() const {
        return m_userName;
    }

    const QnUserResourcePtr &user() const {
        return m_user;
    }

signals:
    void userChanged(const QnUserResourcePtr &user);

private slots:
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    void setCurrentUser(const QnUserResourcePtr &currentUser);

private:
    QString m_userName;
    QnUserResourcePtr m_user;
};
