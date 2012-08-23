#ifndef QN_WORKBENCH_ACCESS_CONTROLLER_H
#define QN_WORKBENCH_ACCESS_CONTROLLER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include "workbench_context_aware.h"
#include "workbench_globals.h"

class QnWorkbenchContext;
class QnResourcePool;

class QnWorkbenchPermissionsNotifier: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchPermissionsNotifier(QObject *parent = NULL): QObject(parent) {}

signals:
    void permissionsChanged(const QnResourcePtr &resource);

private:
    friend class QnWorkbenchAccessController;
};


/**
 * This class implements access control.
 */
class QnWorkbenchAccessController: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchAccessController(QObject *parent = NULL);
    virtual ~QnWorkbenchAccessController();

    Qn::Permissions permissions(const QnResourcePtr &resource) const;
    quint64 rights() const;

    bool hasPermissions(const QnResourcePtr &resource, Qn::Permissions requiredPermissions) const;
    bool hasRights(Qn::UserRights requiredRights) const;

    template<class ResourceList>
    Qn::Permissions permissions(const ResourceList &resources, const typename ResourceList::const_iterator * = NULL /* Let SFINAE filter out non-lists. */) const {
        Qn::Permissions result = Qn::AllPermissions;
        foreach(const QnResourcePtr &resource, resources)
            result &= permissions(resource);
        return result;
    }

    QnWorkbenchPermissionsNotifier *notifier(const QnResourcePtr &resource) const;

signals:
    /**
     * \param resource                  Resource for which permissions have changed. 
     *                                  Guaranteed not to be null.
     */
    void permissionsChanged(const QnResourcePtr &resource);

protected slots:
    void updatePermissions(const QnResourcePtr &resource);
    void updatePermissions(const QnResourceList &resources);
    void updateSenderPermissions();

    void at_context_userChanged(const QnUserResourcePtr &user);
    
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    void at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &layout);

private:
    void setPermissionsInternal(const QnResourcePtr &resource, Qn::Permissions permissions);

    Qn::Permissions calculatePermissions(const QnResourcePtr &resource);
    Qn::Permissions calculatePermissions(const QnUserResourcePtr &user);
    Qn::Permissions calculatePermissions(const QnLayoutResourcePtr &layout);
    Qn::Permissions calculatePermissions(const QnVirtualCameraResourcePtr &camera);
    Qn::Permissions calculatePermissions(const QnAbstractArchiveResourcePtr &archive);
    Qn::Permissions calculatePermissions(const QnVideoServerResourcePtr &server);

private:
    struct PermissionsData {
        PermissionsData(): permissions(0), notifier(NULL) {}

        Qn::Permissions permissions;
        mutable QnWorkbenchPermissionsNotifier *notifier;
    };

    QnUserResourcePtr m_user;
    QHash<QnResourcePtr, PermissionsData> m_dataByResource;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
