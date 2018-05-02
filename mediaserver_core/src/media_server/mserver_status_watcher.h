#pragma once
/**********************************************************
* 22 sep 2014
* a.kolesnikov
***********************************************************/

#include <QObject>
#include <QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>
#include <nx/vms/event/events/events_fwd.h>

//!Monitors mediaserver's status change and sends business event if some server goes offline
class MediaServerStatusWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    MediaServerStatusWatcher(QnCommonModule* commonModule);
    ~MediaServerStatusWatcher();

public slots:
    void at_resource_statusChanged( const QnResourcePtr& resource );
    void at_resource_removed( const QnResourcePtr& resource );
private slots:
    void sendError();
private:
    struct OfflineServerData
    {
        OfflineServerData() { timer.start(); }
        QElapsedTimer timer;
        nx::vms::event::ServerFailureEventPtr serverData;
    };

    QMap<QnUuid, OfflineServerData> m_candidatesToError;
    QSet<QnUuid> m_onlineServers;
};
