#ifndef QN_WORKBENCH_ACCESS_CONTROLLER_H
#define QN_WORKBENCH_ACCESS_CONTROLLER_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QnWorkbenchContext;
class QnResourcePool;

namespace Qn {
    enum Permission {
        /**< Generic read access. Having this access right doesn't necessary mean that all information is readable. */
        ReadPermission              = 0x00000001,   

        /**< Generic write access. Having this access right doesn't necessary mean that all information is writable. */ 
        WritePermission             = 0x00000002,   

        /**< Generic save access. Entity can be saved to appserver. */
        SavePermission              = 0x00000004,   

        /**< Permission to edit user's password. */
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
class QnWorkbenchAccessController: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchAccessController(QObject *parent = NULL);

    virtual ~QnWorkbenchAccessController();

    void setContext(QnWorkbenchContext *context);

    QnWorkbenchContext *context() const {
        return m_context;
    }

    QnResourcePool *resourcePool() const;

    Qn::Permissions permissions(const QnResourcePtr &resource);

protected:
    void start();
    void stop();

    void updateVisibility(const QnUserResourcePtr &user);
    void updateVisibility(const QnUserResourceList &users);

    bool isAdmin() const;

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_context_userChanged(const QnUserResourcePtr &user);
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);

private:
    QnWorkbenchContext *m_context;
    QnUserResourcePtr m_user;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
