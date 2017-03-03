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
#include <network/authenticate_helper.h>

#include <QtCore/QJsonDocument>
#include <QtXmlPatterns/QXmlSchema>
#include <QtXmlPatterns/QXmlSchemaValidator>
#include <QtXml/QDomDocument>


static QnMediaServerResourcePtr m_server;

QByteArray decodeAuthKey(const QByteArray& authKey) {
    // convert from v2.2 format and encode value
    QByteArray prefix("SK_");
    if (authKey.startsWith(prefix)) {
        QByteArray authKeyEncoded = QByteArray::fromHex(authKey.mid(prefix.length()));
        QByteArray authKeyDecoded = QnAuthHelper::symmetricalEncode(authKeyEncoded);
        return QnUuid::fromRfc4122(authKeyDecoded).toByteArray();
    } else {
        return authKey;
    }
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

    const ec2::ErrorCode errorCode = QnAppServerConnectionFactory::getConnection2()->dumpDatabaseToFileSync( fileName );
    if (errorCode != ec2::ErrorCode::ok) {
        NX_LOG(lit("Failed to dump EC database: %1").arg(ec2::toString(errorCode)), cl_logERROR);
        return false;
    }

    return true;
}


bool changeSystemName(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime) {
    if (qnCommon->localSystemName() == systemName)
        return true;

    qnCommon->setLocalSystemName(systemName);
    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (!server) {
        NX_LOG("Cannot find self server resource!", cl_logERROR);
        return false;
    }

    MSSettings::roSettings()->setValue("systemName", systemName);
    qnCommon->setSystemIdentityTime(sysIdTime, qnCommon->moduleGUID());
    server->setSystemName(systemName);
    server->setServerFlags(server->getServerFlags() & ~Qn::SF_AutoSystemName);
    QnAppServerConnectionFactory::getConnection2()->setTransactionLogTime(tranLogTime);
    QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->save(server, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    return true;
}


QByteArray autoDetectHttpContentType(const QByteArray& msgBody)
{
    static const QByteArray kDefaultContentType("text/plain");
    static const QByteArray kJsonContentType("application/json");
    static const QByteArray kXMLContentType("application/xml");
    static const QByteArray kHTMLContentType("text/html; charset=utf-8");

    if (msgBody.isEmpty())
        return QByteArray();

    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(msgBody, &error);
    if(error.error == QJsonParseError::NoError)
        return kJsonContentType;

    const QRegExp regExpr(lit("<html[^<]*>"));

    int pos = regExpr.indexIn(QString::fromUtf8(msgBody));
    while (pos >= 0)
    {
        // Check that <html pattern found not inside string
        int quoteCount = QByteArray::fromRawData(msgBody.data(), pos).count('\"');
        if (quoteCount % 2 == 0)
            return kHTMLContentType;
        pos = regExpr.indexIn(msgBody, pos+1);
    }

    QDomDocument xmlDoc;
    if (xmlDoc.setContent(msgBody))
        return kXMLContentType;

    return kDefaultContentType;
}
