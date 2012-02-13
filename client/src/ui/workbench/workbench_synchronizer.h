#ifndef QN_WORKBENCH_SYNCHRONIZER_H
#define QN_WORKBENCH_SYNCHRONIZER_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QnWorkbench;
class QnWorkbenchLayout;

/**
 * This class performs bidirectional synchronization of instances of 
 * <tt>QnWorkbench</tt> and <tt>QnUserResource</tt>.
 */
class QnWorkbenchSynchronizer: public QObject {
    Q_OBJECT;

public:
    QnWorkbenchSynchronizer(QObject *parent = NULL);

    virtual ~QnWorkbenchSynchronizer();

    QnWorkbench *workbench() {
        return m_workbench;
    }

    const QnUserResourcePtr &user() const {
        return m_user;
    }

signals:
    /**
     * This signal is emitted whenever synchronization starts. For synchronization
     * to start, both workbench and user must be set.
     */
    void started();

    /**
     * This signal is emitted whenever synchronization stops.
     */
    void stopped();

public slots:
    void setWorkbench(QnWorkbench *workbench);
    void setUser(const QnUserResourcePtr &user);
    void update();
    void submit();

protected:
    void start();
    void stop();

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
