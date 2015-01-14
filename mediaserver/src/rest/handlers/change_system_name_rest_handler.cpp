#include "change_system_name_rest_handler.h"

#include "utils/network/tcp_connection_priv.h"
#include "media_server/settings.h"
#include "common/common_module.h"

void restartServer(int restartTimeout);

int QnChangeSystemNameRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)

    QString systemName = params.value(lit("systemName"));
    if (systemName.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter, lit("systemName"));
        return CODE_INVALID_PARAMETER;
    }

    if (qnCommon->localSystemName() != systemName) {
        MSSettings::roSettings()->setValue("systemName", systemName);
        qnCommon->setSystemIdentityTime(0, qnCommon->moduleGUID());
        restartServer(500);
    }

    result.setError(QnJsonRestResult::NoError);
    return CODE_OK;
}
