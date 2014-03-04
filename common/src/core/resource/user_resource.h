#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include <QtCore/QUuid>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>

class QnUserResource : public QnResource
{
    Q_OBJECT

    typedef QnResource base_type;

public:
    QnUserResource();

    virtual QString getUniqueId() const override;

    QString getHash() const;
    void setHash(const QString &hash);

    QString getPassword() const;
    void setPassword(const QString &password);

    QString getDigest() const;
    void setDigest(const QString& digest);

    quint64 getPermissions() const;
    void setPermissions(quint64 permissions);

    bool isAdmin() const;
    void setAdmin(bool isAdmin);

    QString getEmail() const;
    void setEmail(const QString &email);

    /** Returns set of uuids of videowall items the user is able to control. */
    QSet<QUuid> videoWallItems() const;
    void setVideoWallItems(QSet<QUuid> uuids);

    void addVideoWallItem(const QUuid &uuid);
    void removeVideoWallItem(const QUuid &uuid);

signals:
    void emailChanged(const QnUserResourcePtr &user);

    void videoWallItemAdded(const QnUserResourcePtr &user, const QUuid &uuid);
    void videoWallItemRemoved(const QnUserResourcePtr &user, const QUuid &uuid);

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    void addVideoWallItemUnderLock(const QUuid &uuid);
    void removeVideoWallItemUnderLock(const QUuid &uuid);

private:
    QString m_password;
    QString m_hash;
    QString m_digest;
    quint64 m_permissions;
    bool m_isAdmin;
    QString m_email;

    /** Set of uuids of QnVideoWallItems that user is allowed to control. */
    QSet<QUuid> m_videoWallItemUuids;
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)

#endif // QN_USER_RESOURCE_H
