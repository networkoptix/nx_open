#ifndef __VIDEO_SERVER_CONNECTION_H_
#define __VIDEO_SERVER_CONNECTION_H_

#include <QRegion>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QAuthenticator>
#include "recording/device_file_catalog.h"
#include "core/resource/network_resource.h"
#include "utils/common/util.h"

class VideoServerSessionManager;

class QN_EXPORT QnVideoServerConnection: public QObject
{
    Q_OBJECT;
public:
    QnVideoServerConnection(const QHostAddress& host, int port, const QAuthenticator& auth, QObject *parent = NULL);
    virtual ~QnVideoServerConnection();

    QnTimePeriodList recordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs = 0, qint64 endTimeMs = INT64_MAX, qint64 detail = 1, const QRegion& motionRegion = QRegion());

    void asyncRecordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, QRegion motionRegion, QObject *target, const char *slot);

protected:
    QnRequestParamList createParamList(const QnNetworkResourceList& list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QRegion& motionRegion);

private:
    QScopedPointer<VideoServerSessionManager> m_sessionManager;
};

typedef QSharedPointer<QnVideoServerConnection> QnVideoServerConnectionPtr;

#endif // __VIDEO_SERVER_CONNECTION_H_
