#ifndef __VIDEO_SERVER_CONNECTION_P_H_
#define __VIDEO_SERVER_CONNECTION_P_H_

#include "VideoServerConnection.h"
#include "api/Types.h"

class VideoServerSessionManager;

namespace detail {
    class QnVideoServerConnectionReplyProcessor: public QObject {
        Q_OBJECT;
    public:
        QnVideoServerConnectionReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QnApiRecordedTimePeriodsResponsePtr &reply, int handle);

    signals:
        void finished(int, const QnTimePeriodList &timePeriods, int handle);
    };

}

#endif // __VIDEO_SERVER_CONNECTION_P_H_
