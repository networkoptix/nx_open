#ifndef QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H
#define QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H

#include <QObject>
#include <core/resource/user_resource.h>

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;

class QnWorkbenchLayoutSynchronizer: public QObject {
    Q_OBJECT;

public:
    QnWorkbenchLayoutSynchronizer(QObject *parent);

    virtual ~QnWorkbenchLayoutSynchronizer();

    QnWorkbench *workbench() {
        return m_workbench;
    }

    void setWorkbench(QnWorkbench *workbench);

    const QnUserResourcePtr &user() const {
        return m_user;
    }

    void setUser(const QnUserResourcePtr &user);

protected:
    void initUserWorkbench();
    void deinitUserWorkbench();

    QnWorkbenchLayout *layout(const QnLayoutResourcePtr &resource) {
        return m_layoutByResource.value(resource, NULL);
    }

    QnLayoutResourcePtr resource(QnWorkbenchLayout *layout) {
        return m_resourceByLayout.value(layout, QnLayoutResourcePtr());
    }

    void addLayoutResource(QnWorkbenchLayout *layout, const QnLayoutResourcePtr &resource);
    void removeLayoutResource(QnWorkbenchLayout *layout, const QnLayoutResourcePtr &resource);

protected slots:
    void updateLayout(QnWorkbenchLayout *layout, const QnLayoutResourcePtr &resource);
    void submitLayout(QnWorkbenchLayout *layout, const QnLayoutResourcePtr &resource);

    void at_user_resourceChanged();
    void at_layout_resourceChanged();

    void at_workbench_aboutToBeDestroyed();
    void at_workbench_layoutsChanged();
    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);
    void at_layout_nameChanged();

private:
    QnWorkbench *m_workbench;
    QnUserResourcePtr m_user;

    /** Whether changes should be propagated from workbench to resources. */
    bool m_submit;

    /** Whether changes should be propagated from resources to workbench. */
    bool m_update;

    QHash<QnWorkbenchLayout *, QnLayoutResourcePtr> m_resourceByLayout;
    QHash<QnLayoutResourcePtr, QnWorkbenchLayout *> m_layoutByResource;
};

#endif // QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H
