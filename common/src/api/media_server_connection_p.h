#ifndef __VIDEO_SERVER_CONNECTION_P_H_
#define __VIDEO_SERVER_CONNECTION_P_H_

#include "media_server_connection.h"

namespace detail {
    class QnMediaServerConnectionReplyProcessor: public QObject {
        Q_OBJECT;
    public:
        QnMediaServerConnectionReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QnTimePeriodList &result, int handle);

    signals:
        void finished(int, const QnTimePeriodList &timePeriods, int handle);
    };

}

#endif // __VIDEO_SERVER_CONNECTION_P_H_
