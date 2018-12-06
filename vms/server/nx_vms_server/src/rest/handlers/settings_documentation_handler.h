#pragma once

#include "rest/server/request_handler.h"
#include "rest/server/json_rest_handler.h"

class QnGlobalMonitor;

namespace nx { namespace vms::server { class Settings; }}

class QnSettingsDocumentationHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnSettingsDocumentationHandler(const nx::vms::server::Settings* settings);

    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*) override;
private:
    const nx::vms::server::Settings* m_settings = nullptr;
};
