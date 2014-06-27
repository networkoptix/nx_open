#ifndef WORKBENCH_SYSTEM_NAME_WATCHER_H
#define WORKBENCH_SYSTEM_NAME_WATCHER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

class QnWorkbenchSystemNameWatcher : public QObject {
    Q_OBJECT
public:
    QnWorkbenchSystemNameWatcher(QObject *parent = 0);
    ~QnWorkbenchSystemNameWatcher();

private slots:
    void at_resourceChanged(const QnResourcePtr &resource);
};

#endif // WORKBENCH_SYSTEM_NAME_WATCHER_H
