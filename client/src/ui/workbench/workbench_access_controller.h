#ifndef QN_WORKBENCH_ACCESS_CONTROLLER_H
#define QN_WORKBENCH_ACCESS_CONTROLLER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include "workbench_context_aware.h"

class QnWorkbenchContext;
class QnResourcePool;

namespace Qn {
    enum Permission {
        /** Generic read access. Having this access right doesn't necessary mean that all information is readable. */
        ReadPermission              = 0x00000001,   

        /** Generic write access. Having this access right doesn't necessary mean that all information is writable. */ 
        WritePermission             = 0x00000002,   

        /** Generic save access. Entity can be saved to appserver. */
        SavePermission              = 0x00000004,   

        /** Generic delete permission. */
        DeletePermission            = 0x00000008,

        /** Generic read-write-save permission. */
        ReadWriteSavePermission     = ReadPermission | WritePermission | SavePermission,



        /** Permission to view associated password. Note that not all passwords are viewable even with this flag. */
        ReadPasswordPermission      = 0x00000010,



        /** Permission to edit associated password. */
        WritePasswordPermission     = 0x00000100,

        /** Permission to edit access rights. */
        WriteAccessRightsPermission = 0x00000200,


        /** Permission to create users. */
        CreateUserPermission        = 0x00010000,
    };

    Q_DECLARE_FLAGS(Permissions, Permission);

} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::Permissions);


/**
 * This class implements access control.
 * 
 * It hides resources that cannot be viewed by the user that is currently logged in.
 */
class QnWorkbenchAccessController: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchAccessController(QObject *parent = NULL);
    virtual ~QnWorkbenchAccessController();

    Qn::Permissions permissions(const QnResourcePtr &resource = QnResourcePtr()) {
        return m_permissionsByResource.value(resource);
    }

signals:
    void permissionsChanged(const QnResourcePtr &resource);

protected:
    bool isOwner() const;
    bool isAdmin() const;

protected slots:
    void updatePermissions(const QnResourcePtr &resource);
    void updatePermissions(const QnResourceList &resources);

    void at_context_userChanged(const QnUserResourcePtr &user);
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    void setPermissionsInternal(const QnResourcePtr &resource, Qn::Permissions permissions);

    Qn::Permissions calculateGlobalPermissions();
    Qn::Permissions calculatePermissions(const QnResourcePtr &resource);
    Qn::Permissions calculatePermissions(const QnUserResourcePtr &user);
    Qn::Permissions calculatePermissions(const QnLayoutResourcePtr &layout);

private:
    QnUserResourcePtr m_user;
    QHash<QnResourcePtr, Qn::Permissions> m_permissionsByResource;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
