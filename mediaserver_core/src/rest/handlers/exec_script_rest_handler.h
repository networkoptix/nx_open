#pragma once

#include "rest/server/json_rest_handler.h"


class QnExecScript: public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnExecScript(const QString& dataDirectory);

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    void afterExecute(const QString &path, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor* owner) override;
private:
    const QString m_dataDirectory;
};
