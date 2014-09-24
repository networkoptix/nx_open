/**********************************************************
* 22 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_MSERVER_STATUS_WATCHER_H
#define NX_MSERVER_STATUS_WATCHER_H

#include <QObject>

#include <core/resource/resource_fwd.h>


//!Monitors mediaserver's status change and sends business event if some server goes offline
class MediaServerStatusWatcher
:
    public QObject
{
    Q_OBJECT

public:
    MediaServerStatusWatcher();
    ~MediaServerStatusWatcher();

public slots:
    void at_resource_statusChanged( const QnResourcePtr& resource );
};

#endif  //NX_MSERVER_STATUS_WATCHER_H
