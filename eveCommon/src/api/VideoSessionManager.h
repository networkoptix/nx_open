#ifndef __VIDEO_SERVER_SESSION_MANAGER_H__
#define __VIDEO_SERVER_SESSION_MANAGER_H__

#include "SessionManager.h"
#include "Types.h"

class VideoServerSessionManager: public SessionManager
{
    int getTimePeriods(const QnRequestParamList& params, QnApiTimePeriodListResponsePtr& timePeriodList);
};

#endif // __VIDEO_SERVER_SESSION_MANAGER_H__
