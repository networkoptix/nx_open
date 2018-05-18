#pragma once

#include "rest/server/json_rest_handler.h"

class QnStartLiteClientRestHandler:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    /**
     * @return Whether Lite Client is present in the installation. The exact semantics of the check
     * is encapsulated inside the implementation of this method.
     */
    static bool isLiteClientPresent();

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* connectionProcessor) override;
};
