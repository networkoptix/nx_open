#pragma once

#include <rest/server/json_rest_handler.h>

class QnCookieLoginRestHandler: public QnJsonRestHandler 
{
    Q_OBJECT
public:
    virtual JsonRestResponse executePost(
        const JsonRestRequest& request, const QByteArray& body) override;
};
