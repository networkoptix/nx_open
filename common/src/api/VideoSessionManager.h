#ifndef __VIDEO_SERVER_SESSION_MANAGER_H__
#define __VIDEO_SERVER_SESSION_MANAGER_H__

#include "SessionManager.h"
#include "Types.h"

class VideoServerSessionManager: public SessionManager
{
public:
    VideoServerSessionManager(const QHostAddress& host, int port, const QAuthenticator& auth);

    int recordedTimePeriods(const QnRequestParamList& params, QnApiRecordedTimePeriodsResponsePtr& timePeriodList);

private:
    static const unsigned long XSD_FLAGS = xml_schema::flags::dont_initialize | xml_schema::flags::dont_validate;
};

#endif // __VIDEO_SERVER_SESSION_MANAGER_H__
