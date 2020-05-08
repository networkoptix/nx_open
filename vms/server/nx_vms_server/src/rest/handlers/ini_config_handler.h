#pragma once

#include <rest/server/request_handler.h>

class QnIniConfigHandler: public QnRestRequestHandler
{
protected:
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& responseMessageBody,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override;
};
