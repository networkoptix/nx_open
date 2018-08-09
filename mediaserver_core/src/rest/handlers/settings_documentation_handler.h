#pragma once

#include "rest/server/request_handler.h"
#include "rest/server/json_rest_handler.h"

class QnGlobalMonitor;

namespace nx { namespace mediaserver { class Settings; }}

class QnSettingsDocumentationHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnSettingsDocumentationHandler(const nx::mediaserver::Settings* settings);

    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*) override;
private:
    const nx::mediaserver::Settings* m_settings = nullptr;
};

