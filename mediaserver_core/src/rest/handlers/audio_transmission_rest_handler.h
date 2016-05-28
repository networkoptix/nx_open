#pragma once

#include <rest/server/json_rest_handler.h>
#include <utils/network/abstract_socket.h>

class QnAudioTransmissionRestHandler : public QnJsonRestHandler
{

public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor*) override;

    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;

    virtual void afterExecute(const nx_http::Request& request, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor* owner) override;
private:
    bool readHttpHeaders(QSharedPointer<AbstractStreamSocket> socket);
private:
    bool validateParams(const QnRequestParams& params, QString& error) const;

};

