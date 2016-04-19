#ifndef AUDIO_TRANSMISSION_REST_HANDLER_H
#define AUDIO_TRANSMISSION_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnAudioTransmissionRestHandler : public QnJsonRestHandler
{

public:
    static const QString kClientIdParamName;
    static const QString kResourceIdParamName;
    static const QString kActionParamName;
    static const QString kStartStreamAction;
    static const QString kStopStreamAction;

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;

private:
    bool validateParams(const QnRequestParamList& params) const;

};

#endif // AUDIO_TRANSMISSION_REST_HANDLER_H
