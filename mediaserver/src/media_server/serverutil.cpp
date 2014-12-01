#include <QtCore/QString>
#include <utils/common/uuid.h>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
#include <nx_ec/dummy_handler.h>
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <utils/common/log.h>
#include <utils/common/app_info.h>
#include <common/common_module.h>

static QnMediaServerResourcePtr m_server;

QString authKey()
{
    return MSSettings::roSettings()->value("authKey").toString();
}

QnUuid serverGuid() {
    static QnUuid guid;

    if (!guid.isNull())
        return guid;

    guid = QnUuid(MSSettings::roSettings()->value(lit("serverGuid")).toString());

    return guid;
}

bool isLocalAppServer(const QString &host) {
    return host.isEmpty() || host == "localhost" || host == "127.0.0.1" || QUrl(host).scheme() == "file";
}

QString getDataDirectory()
{
    const QString& dataDirFromSettings = MSSettings::roSettings()->value( "dataDir" ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/mediaserver/var").arg(QnAppInfo::linuxOrganizationName());
    QString varDirName = MSSettings::roSettings()->value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}


bool backupDatabase() {
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
        return false;

    QByteArray data;
    ec2::ErrorCode errorCode;

    QEventLoop loop;
    auto dumpDatabaseHandler =
        [&loop, &errorCode, &data] (int /*reqID*/, ec2::ErrorCode _errorCode, const ec2::ApiDatabaseDumpData &dumpData) {
            errorCode = _errorCode;
            data = dumpData.data;
            loop.quit();
    };

    QnAppServerConnectionFactory::getConnection2()->dumpDatabaseAsync(&loop, dumpDatabaseHandler);
    loop.exec();

    if (errorCode != ec2::ErrorCode::ok) {
        NX_LOG(lit("Failed to dump EC database: %1").arg(ec2::toString(errorCode)), cl_logERROR);
        return false;
    }

    file.write(data);
    file.close();

    return true;
}


bool changeSystemName(const QString &systemName) {
    if (qnCommon->localSystemName() == systemName)
        return true;

    qnCommon->setLocalSystemName(systemName);
    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (!server) {
        NX_LOG("Cannot find self server resource!", cl_logERROR);
        return false;
    }

    MSSettings::roSettings()->setValue("systemName", systemName);
    server->setSystemName(systemName);
    QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->save(server, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    return true;
}
