#pragma once

#include <rest/server/json_rest_handler.h>

class QnCookieLogoutRestHandler : public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual RestResponse executeGet(const RestRequest& request);
    virtual RestResponse executePost(const RestRequest& request);

    static void logout(const QnRestConnectionProcessor* connection);
};
