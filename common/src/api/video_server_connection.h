#ifndef __VIDEO_SERVER_CONNECTION_H_
#define __VIDEO_SERVER_CONNECTION_H_

#include <QUrl>
#include <QRegion>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QAuthenticator>
#include "core/resource/network_resource.h"
#include "utils/common/util.h"
#include "recording/time_period.h"

class VideoServerSessionManager;

namespace detail {
    class VideoServerSessionManagerReplyProcessor: public QObject
    {
        Q_OBJECT;
    public:
        VideoServerSessionManagerReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString,int handle);

    signals:
        void finished(int status, const QnTimePeriodList& timePeriods, int handle);
    };

    class VideoServerSessionManagerFreeSpaceRequestReplyProcessor: public QObject
    {
        Q_OBJECT;
    public:
        VideoServerSessionManagerFreeSpaceRequestReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString,int handle);

    signals:
        void finished(int status, qint64 freeSpace, qint64 usedSpace, int handle);
    };

    class VideoServerSessionManagerStatisticsRequestReplyProcessor: public QObject
    {
        Q_OBJECT;
    public:
        VideoServerSessionManagerStatisticsRequestReplyProcessor(QObject *parent = NULL): QObject(parent) {}
    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray /* &errorString */ , int /*handle*/);
    signals:
        void finished(int /* usage */ , const QByteArray & /* model */);
    };
}

class QN_EXPORT QnVideoServerConnection: public QObject
{
    Q_OBJECT;
public:
    QnVideoServerConnection(const QUrl &url, QObject *parent = 0);
    virtual ~QnVideoServerConnection();

    QnTimePeriodList recordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs = 0, qint64 endTimeMs = INT64_MAX, qint64 detail = 1, const QList<QRegion>& motionRegions = QList<QRegion>());

    int asyncRecordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, QList<QRegion> motionRegions, QObject *target, const char *slot);

    int asyncGetFreeSpace(const QString& path, QObject *target, const char *slot);

    static void setProxyAddr(const QString& addr, int port);
    static int getProxyPort() { return m_proxyPort; }
    static QString getProxyHost() { return m_proxyAddr; }

    /** Returns request handle */
    int asyncGetStatistics(QObject *target, const char *slot);

    /** Returns status */
    int syncGetStatistics(QObject *target, const char *slot);
private:
    int recordedTimePeriods(const QnRequestParamList& params, QnTimePeriodList& timePeriodList, QByteArray& errorString);

    int asyncRecordedTimePeriods(const QnRequestParamList& params, QObject *target, const char *slot);

protected:
    QnRequestParamList createParamList(const QnNetworkResourceList& list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion>& motionRegions);

private:
    QUrl m_url;
    static QString m_proxyAddr;
    static int m_proxyPort;
};

typedef QSharedPointer<QnVideoServerConnection> QnVideoServerConnectionPtr;

#endif // __VIDEO_SERVER_CONNECTION_H_
