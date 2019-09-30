#pragma once

#include "rest/server/request_handler.h"
#include "rest/server/json_rest_handler.h"

namespace nx::vms::server {
    class Settings;
    class GlobalMonitor;
} // namespace nx::vms::server

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
