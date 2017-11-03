#pragma once

#include <rest/server/json_rest_handler.h>

class QnGetNonceRestHandler: public QnJsonRestHandler
{
public:
    QnGetNonceRestHandler(bool isUrlSupported);

    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*) override;

private:
    bool m_isUrlSupported = false;
};
