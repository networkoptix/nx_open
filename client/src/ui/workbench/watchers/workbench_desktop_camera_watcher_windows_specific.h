#ifndef QN_WORKBENCH_DESKTOP_CAMERA_WATCHER_H
#define QN_WORKBENCH_DESKTOP_CAMERA_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QDateTime>
#include <QtCore/QBasicTimer>

#include <common/common_globals.h>
#include <utils/math/math.h>
#include <core/resource/resource_fwd.h>
#include <api/model/time_reply.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchDesktopCameraWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;

    typedef QObject base_type;

public:
    QnWorkbenchDesktopCameraWatcher(QObject *parent);
    virtual ~QnWorkbenchDesktopCameraWatcher();
private slots:
    void at_server_serverIfFound(const QnMediaServerResourcePtr &resource);
    void at_resource_statusChanged(const QnResourcePtr &resource);

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
private:
    void processServer(QnMediaServerResourcePtr server);
private:
    QSet<QnMediaServerResourcePtr> m_serverList;
};

#endif // QN_WORKBENCH_DESKTOP_CAMERA_WATCHER_H
