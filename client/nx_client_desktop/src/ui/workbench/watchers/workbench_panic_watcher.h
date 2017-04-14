#ifndef QN_WORKBENCH_PANIC_WATCHER_H
#define QN_WORKBENCH_PANIC_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchPanicWatcher: public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnWorkbenchPanicWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchPanicWatcher();

    bool isPanicMode() const {
        return m_panicMode;
    }

signals:
    void panicModeChanged();

private slots:
    void updatePanicMode();

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
private:
    bool m_panicMode;
};



#endif // QN_WORKBENCH_PANIC_WATCHER_H
