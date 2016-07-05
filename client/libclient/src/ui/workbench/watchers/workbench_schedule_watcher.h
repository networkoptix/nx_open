#ifndef QN_WORKBENCH_SCHEDULE_WATCHER_H
#define QN_WORKBENCH_SCHEDULE_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchScheduleWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchScheduleWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchScheduleWatcher();
    
    bool isScheduleEnabled() const;

signals:
    void scheduleEnabledChanged();

private:
    void updateScheduleEnabled();

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_resource_scheduleDisabledChanged(const QnSecurityCamResourcePtr &resource);

private:
    bool m_scheduleEnabled;
    QSet<QnSecurityCamResourcePtr> m_scheduleEnabledCameras;
};


#endif // QN_WORKBENCH_SCHEDULE_WATCHER_H
