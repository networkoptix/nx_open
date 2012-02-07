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

protected slots:
    void at_user_resourceChanged();
    void at_layout_resourceChanged();

    void at_workbench_aboutToBeDestroyed();
    void at_workbench_layoutsChanged();
    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_itemAdded(QnWorkbenchItem *item);
    void at_workbench_itemRemoved(QnWorkbenchItem *item);

private:
    QnWorkbench *m_workbench;
    QnUserResourcePtr m_user;

    QHash<QnWorkbenchLayout *, QnLayoutDataPtr> m_resourceByLayout;
    QHash<QnLayoutDataPtr, QnWorkbenchLayout *> m_layoutByResource;
};

#endif // QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H
