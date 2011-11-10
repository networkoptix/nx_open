#ifndef __VIDEO_SERVER_CONNECTION_H_
#define __VIDEO_SERVER_CONNECTION_H_

#include <QSharedPointer>
#include <QAuthenticator>
#include "recording/device_file_catalog.h"
#include "core/resource/network_resource.h"

class VideoServerSessionManager;
class QN_EXPORT QnVideoServerConnection
{
public:
    QnVideoServerConnection(const QHostAddress& host, int port, const QAuthenticator& auth);

    QnTimePeriodList recordedTimePeriods(const QnNetworkResourceList& list, qint64 startTime, qint64 endTime, qint64 detail);

private:
    QSharedPointer<VideoServerSessionManager> m_sessionManager;
};

typedef QSharedPointer<QnVideoServerConnection> QnVideoServerConnectionPtr;

#endif // __VIDEO_SERVER_CONNECTION_H_
