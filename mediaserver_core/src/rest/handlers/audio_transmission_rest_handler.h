#pragma once

#include <rest/server/json_rest_handler.h>

class QnAudioTransmissionRestHandler : public QnJsonRestHandler
{

public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor*) override;


private:
    bool validateParams(const QnRequestParams& params, QString& error) const;

};

