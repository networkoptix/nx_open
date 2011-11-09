#ifndef __VIDEO_SERVER_SESSION_MANAGER_H__
#define __VIDEO_SERVER_SESSION_MANAGER_H__

#include "SessionManager.h"
#include "Types.h"

class VideoServerSessionManager: public SessionManager
{
public:
    VideoServerSessionManager(const QHostAddress& host, int port, const QAuthenticator& auth);

    int recordedTimePeriods(const QnRequestParamList& params, QnApiRecordedTimePeriodsResponsePtr& timePeriodList);
};

#endif // __VIDEO_SERVER_SESSION_MANAGER_H__
