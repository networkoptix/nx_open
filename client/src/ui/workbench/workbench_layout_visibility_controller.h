#ifndef QN_WORKBENCH_LAYOUT_VISIBILITY_CONTROLLER_H
#define QN_WORKBENCH_LAYOUT_VISIBILITY_CONTROLLER_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QnWorkbenchContext;
class QnWorkbenchLayoutSnapshotManager;

/**
 * This class implements hiding of layouts that are not saved to application server. 
 */
class QnWorkbenchLayoutVisibilityController: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchLayoutVisibilityController(QObject *parent = NULL);

    virtual ~QnWorkbenchLayoutVisibilityController();

    void setContext(QnWorkbenchContext *context);

    QnWorkbenchContext *context() const {
        return m_context;
    }

    QnWorkbenchLayoutSnapshotManager *snapshotManager() const;

protected:
    void start();
    void stop();

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource);

private:
    QnWorkbenchContext *m_context;
};

#endif // QN_WORKBENCH_LAYOUT_VISIBILITY_CONTROLLER_H
