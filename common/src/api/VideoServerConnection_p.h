#ifndef __VIDEO_SERVER_CONNECTION_P_H_
#define __VIDEO_SERVER_CONNECTION_P_H_

#include "VideoServerConnection.h"

class VideoServerSessionManager;

namespace detail {
    class QnVideoServerConnectionReplyProcessor: public QObject {
        Q_OBJECT;
    public:
        QnVideoServerConnectionReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QnTimePeriodList &result);

    signals:
        void finished(int, const QnTimePeriodList &timePeriods);
    };

}

#endif // __VIDEO_SERVER_CONNECTION_P_H_
