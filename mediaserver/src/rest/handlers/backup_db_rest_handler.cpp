#include "backup_db_rest_handler.h"

#include <QtCore/QFile>
#include <QtCore/QEventLoop>

#include "nx_ec/ec_api.h"
#include "nx_ec/dummy_handler.h"
#include "api/app_server_connection.h"

#include "utils/network/tcp_connection_priv.h"
#include "media_server/serverutil.h"

int QnBackupDbRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)
    Q_UNUSED(result)
    Q_UNUSED(params)

    QString dir = getDataDirectory() + lit("/");
    QString fileName;
    for (int i = -1; ; i++) {
        QString suffix = (i < 0) ? QString() : lit("_") + QString::number(i);
        fileName = dir + lit("ecs") + suffix + lit(".backup");
        if (!QFile::exists(fileName))
            break;
    }

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly))
        return CODE_INTERNAL_ERROR;

    QByteArray data;
    ec2::ErrorCode errorCode;

    QEventLoop loop;
    auto dumpDatabaseHandler =
        [&loop, &errorCode, &data] (int /*reqID*/, ec2::ErrorCode _errorCode, const QByteArray &dbData) {
            errorCode = _errorCode;
            data = dbData;
            loop.quit();
    };

    QnAppServerConnectionFactory::getConnection2()->dumpDatabaseAsync(&loop, dumpDatabaseHandler);
    loop.exec();

    if (errorCode != ec2::ErrorCode::ok) {
        NX_LOG(lit("Failed to dump EC database: %1").arg(ec2::toString(errorCode)), cl_logERROR);
        return CODE_INTERNAL_ERROR;
    }

    file.write(data);
    file.close();

    return CODE_OK;
}
