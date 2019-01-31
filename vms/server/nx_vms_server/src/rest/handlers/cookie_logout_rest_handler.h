#pragma once

#include <rest/server/json_rest_handler.h>

class QnCookieLogoutRestHandler : public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual JsonRestResponse executeGet(const JsonRestRequest& request);
    virtual JsonRestResponse executePost(const JsonRestRequest& request, const QByteArray& body);

    static void logout(const QnRestConnectionProcessor* connection);
};
