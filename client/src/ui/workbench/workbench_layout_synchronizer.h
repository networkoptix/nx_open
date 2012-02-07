#ifndef QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H
#define QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H

#include <QObject>
#include <core/resource/user.h>

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

    QnWorkbenchLayout *layout(const QnLayoutDataPtr &resource) {
        return m_layoutByResource.value(resource, NULL);
    }

    QnLayoutDataPtr resource(QnWorkbenchLayout *layout) {
        return m_resourceByLayout.value(layout, QnLayoutDataPtr());
    }

    void addLayoutResource(QnWorkbenchLayout *layout, const QnLayoutDataPtr &resource);
    void removeLayoutResource(QnWorkbenchLayout *layout, const QnLayoutDataPtr &resource);

protected slots:
    void updateLayout(QnWorkbenchLayout *layout, const QnLayoutDataPtr &resource);
    void submitLayout(QnWorkbenchLayout *layout, const QnLayoutDataPtr &resource);

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

    QHash<QnWorkbenchLayout *, QnLayoutDataPtr> m_resourceByLayout;
    QHash<QnLayoutDataPtr, QnWorkbenchLayout *> m_layoutByResource;
};

#endif // QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H
