#ifndef QN_WORKBENCH_PTZ_MAPPER_WATCHER_H
#define QN_WORKBENCH_PTZ_MAPPER_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

#include <ui/workbench/workbench_context_aware.h>

class QnPtzSpaceMapper;
class QnWorkbenchPtzCameraWatcher;

class QnWorkbenchPtzMapperWatcher: public AdlConnective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef AdlConnective<QObject> base_type;

public:
    QnWorkbenchPtzMapperWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzMapperWatcher();

    const QnPtzSpaceMapper *mapper(const QnVirtualCameraResourcePtr &resource) const;

signals:
    void mapperChanged(const QnVirtualCameraResourcePtr &resource);

private:
    void setMapper(const QnVirtualCameraResourcePtr &resource, const QnPtzSpaceMapper *mapper);

    void sendRequest(const QnMediaServerResourcePtr &server, const QnVirtualCameraResourcePtr &camera);

private slots:
    void at_ptzWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera);
    void at_ptzWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera);
    void at_resource_statusChanged(const QnResourcePtr &resource);
    void at_server_serverIfFound(const QnMediaServerResourcePtr &server);
    void at_server_statusChanged(const QnResourcePtr &resource);
    void at_replyReceived(int status, const QnPtzSpaceMapper &mapper, int handle);

private:
    QnWorkbenchPtzCameraWatcher *m_ptzCamerasWatcher;
    QHash<QnVirtualCameraResourcePtr, const QnPtzSpaceMapper *> m_mapperByResource;
    QHash<int, QnVirtualCameraResourcePtr> m_resourceByHandle;
    QSet<QnVirtualCameraResourcePtr> m_requests;
};


#endif // QN_WORKBENCH_PTZ_MAPPER_WATCHER_H
