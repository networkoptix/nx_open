#pragma once

#include <rest/server/json_rest_handler.h>

class QnCookieLoginRestHandler: public QnJsonRestHandler 
{
    Q_OBJECT
public:
    virtual virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};
