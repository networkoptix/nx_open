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


        /** Permission to view associated password. Note that not all passwords are viewable even with this flag. */
        ReadPasswordPermission      = 0x00000008,

        /** Permission to edit associated password. */
        WritePasswordPermission     = 0x00000010,
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

    Qn::Permissions permissions(const QnResourcePtr &resource);

protected:
    void start();
    void stop();

    void updateVisibility(const QnUserResourcePtr &user);
    void updateVisibility(const QnUserResourceList &users);

    bool isAdmin() const;

protected slots:
    void at_context_userChanged(const QnUserResourcePtr &user);
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);

private:
    QnUserResourcePtr m_user;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
