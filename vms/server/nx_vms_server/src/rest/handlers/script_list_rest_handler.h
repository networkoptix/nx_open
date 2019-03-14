#pragma once

#include "rest/server/json_rest_handler.h"

class QnScriptListRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnScriptListRestHandler(const QString& dataDir);

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    const QString m_dataDir;
};
