#pragma once

#include <rest/server/json_rest_handler.h>
#include <utils/network/abstract_socket.h>

class QnProxyDesktopDataProvider;

class QnAudioTransmissionRestHandler : public QnJsonRestHandler
{

public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor*) override;

    static bool validateParams(const QnRequestParams& params, QString& error);
private:
    bool readHttpHeaders(QSharedPointer<AbstractStreamSocket> socket);
private:
};

