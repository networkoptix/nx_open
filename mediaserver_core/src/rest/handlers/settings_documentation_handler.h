#pragma once

#include "rest/server/request_handler.h"
#include "rest/server/json_rest_handler.h"

class QnGlobalMonitor;

class QnSettingsDocumentationHandler: public QnJsonRestHandler {
    Q_OBJECT

public:
    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*) override;
};

