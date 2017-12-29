#pragma once

#include <rest/server/json_rest_handler.h>

class QnCookieHelper
{
public:
    static void addLogoutHeaders(nx::network::http::HttpHeaders* outHeaders);
};

class QnCookieLogoutRestHandler : public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};
