#ifndef QN_WORKBENCH_SYNCHRONIZER_H
#define QN_WORKBENCH_SYNCHRONIZER_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QnWorkbench;
class QnWorkbenchLayout;

class QnWorkbenchSynchronizer: public QObject {
    Q_OBJECT;

public:
    QnWorkbenchSynchronizer(QObject *parent = NULL);

    virtual ~QnWorkbenchSynchronizer();

    QnWorkbench *workbench() {
        return m_workbench;
    }

    void setWorkbench(QnWorkbench *workbench);

    const QnUserResourcePtr &user() const {
        return m_user;
    }

    void setUser(const QnUserResourcePtr &user);

public slots:
    void update();
    void submit();

protected:
    void initialize();
    void deinitialize();

    QnLayoutResourcePtr resource(QnWorkbenchLayout *layout) const;
    QnWorkbenchLayout *layout(const QnLayoutResourcePtr &resource) const;

protected slots:
    void at_user_resourceChanged();
    void at_workbench_aboutToBeDestroyed();
    void at_workbench_layoutsChanged();

private:
    /** Associated workbench. */
    QnWorkbench *m_workbench;

    /** Associated user resource. */
    QnUserResourcePtr m_user;

    /** Whether changes should be propagated from workbench to resources. */
    bool m_submit;

    /** Whether changes should be propagated from resources to workbench. */
    bool m_update;
};


#endif // QN_WORKBENCH_SYNCHRONIZER_H
