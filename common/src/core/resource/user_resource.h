#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include "resource.h"
#include "layout_resource.h"

namespace Qn {
    /**
     * Flags describing the actions permitted for the current user.
     */
    enum UserRight{
        EditProtectedUserRight      = 0x00000001,       // super-admin, can edit even admins
        ProtectedRight              = 0x00000002,       // admin, cannot be edited by other admins

        EditLayoutRight             = 0x00000004,       // can create and edit layouts
        EditUserRight               = 0x00000008,       // can create and edit users
        EditCameraRight             = 0x00000010,       // can edit camera schedule, can setup PTZ
        EditServerRight             = 0x00000020,       // can view servers list and edit server settings

        ViewArchiveRight            = 0x00000040,       // can view archives of available cameras

        /* Combined rights */
        LiveViewerRight             = 0,
        ViewerRight                 = LiveViewerRight       | ViewArchiveRight,
        AdvancedViewerRight         = ViewerRight           | EditCameraRight,
        AdminRight                  = AdvancedViewerRight   | EditLayoutRight | EditUserRight | ProtectedRight | EditServerRight,
        OwnerRight                  = AdminRight            | EditProtectedUserRight

    };
    Q_DECLARE_FLAGS(UserRights, UserRight)
} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::UserRights)

class QnUserResource : public QnResource
{
    Q_OBJECT

    typedef QnResource base_type;

public:
    QnUserResource();

    virtual QString getUniqueId() const override;

    QString getPassword() const;    
    void setPassword(const QString &password);

    quint64 getRights() const;
    void setRights(quint64 rights);

    bool isAdmin() const;
    void setAdmin(bool isAdmin);
protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    QString m_password;
    quint64 m_rights;
    bool m_isAdmin;
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)

#endif // QN_USER_RESOURCE_H
