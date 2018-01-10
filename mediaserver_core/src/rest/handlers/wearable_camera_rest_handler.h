#pragma once

#include <rest/server/json_rest_handler.h>

class QnWearableCameraRestHandler : public QnJsonRestHandler
{
    Q_OBJECT

public:
    virtual QStringList cameraIdUrlParams() const override;

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

private:
    int executeAdd(const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner);
    int executeConsume(const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner);
};
