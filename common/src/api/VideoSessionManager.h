#ifndef __VIDEO_SERVER_SESSION_MANAGER_H__
#define __VIDEO_SERVER_SESSION_MANAGER_H__

#include "SessionManager.h"
#include "Types.h"

namespace detail {
    class VideoServerSessionManagerReplyProcessor: public QObject
    {
        Q_OBJECT;
    public:
        VideoServerSessionManagerReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply);

    signals:
        void finished(int status, const QnApiRecordedTimePeriodsResponsePtr &timePeriods);
    };
}

class VideoServerSessionManager: public SessionManager
{
    Q_OBJECT

public:
    VideoServerSessionManager(const QUrl &url, QObject *parent = 0);

    int recordedTimePeriods(const QnRequestParamList& params, QnApiRecordedTimePeriodsResponsePtr& timePeriodList);

    void asyncRecordedTimePeriods(const QnRequestParamList& params, QObject *target, const char *slot);
};

#endif // __VIDEO_SERVER_SESSION_MANAGER_H__
