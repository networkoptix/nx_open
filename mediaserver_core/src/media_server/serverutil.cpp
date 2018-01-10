#include <QtCore/QString>
#include <nx/utils/uuid.h>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <nx_ec/managers/abstract_server_manager.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/dummy_handler.h>
#include <nx/kit/debug.h>

#include <media_server/serverutil.h>
#include <media_server/settings.h>

#include <utils/common/app_info.h>
#include <common/common_module.h>

#include <network/authenticate_helper.h>
#include <network/system_helpers.h>

#include <nx/network/nettools.h>

#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <core/resource/user_resource.h>
#include <server/host_system_password_synchronizer.h>
#include <core/resource_management/resource_properties.h>

#include <nx/fusion/model_functions.h>
#include <transaction/message_bus_adapter.h>
#include <core/resource_access/resource_access_manager.h>
#include <network/authutil.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <api/resource_property_adaptor.h>

#include <QtCore/QJsonDocument>
#include <QtXmlPatterns/QXmlSchema>
#include <QtXmlPatterns/QXmlSchemaValidator>
#include <QtXml/QDomDocument>

#include "server_connector.h"
#include "server/server_globals.h"
#include <media_server/media_server_module.h>
#include <utils/crypt/symmetrical.h>
#include <api/model/password_data.h>

namespace
{
    static const QByteArray SYSTEM_NAME_KEY("systemName");
    static const QByteArray PREV_SYSTEM_NAME_KEY("prevSystemName");
    static const QByteArray AUTO_GEN_SYSTEM_NAME("__auto__");
    static const QByteArray SYSTEM_IDENTITY_TIME("sysIdTime");
    static const QByteArray AUTH_KEY("authKey");
}

static QnMediaServerResourcePtr m_server;

QnUuid serverGuid()
{
    static QnUuid guid;

    if (!guid.isNull())
        return guid;

    guid = QnUuid(qnServerModule->roSettings()->value(lit("serverGuid")).toString());

    return guid;
}

bool isLocalAppServer(const QString &host) {
    return host.isEmpty() || host == "localhost" || host == "127.0.0.1" || QUrl(host).scheme() == "file";
}

QString getDataDirectory()
{
    const QString& dataDirFromSettings = qnServerModule->roSettings()->value( "dataDir" ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/mediaserver/var").arg(QnAppInfo::linuxOrganizationName());
    QString varDirName = qnServerModule->roSettings()->value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

bool updateUserCredentials(
    std::shared_ptr<ec2::AbstractECConnection> connection,
    PasswordData data,
    QnOptionalBool isEnabled,
    const QnUserResourcePtr& userRes,
    QString* errString)
{
    if (!userRes)
    {
        if (errString)
            *errString = lit("Temporary unavailable. Please try later.");
        return false;
    }

    ec2::ApiUserData apiOldUser;
    fromResourceToApi(userRes, apiOldUser);

    //generating cryptSha512Hash
    if (data.cryptSha512Hash.isEmpty() && !data.password.isEmpty())
        data.cryptSha512Hash = linuxCryptSha512(data.password.toUtf8(), generateSalt(LINUX_CRYPT_SALT_LENGTH));

    //making copy of admin user to be able to rollback local changed on DB update failure
    QnUserResourcePtr updatedUser = QnUserResourcePtr(new QnUserResource(*userRes));

    if (data.password.isEmpty() &&
        updatedUser->getHash() == data.passwordHash &&
        updatedUser->getDigest() == data.passwordDigest &&
        updatedUser->getCryptSha512Hash() == data.cryptSha512Hash &&
        (!isEnabled.isDefined() || updatedUser->isEnabled() == isEnabled.value()))
    {
        //no need to update anything
        return true;
    }

    if (isEnabled.isDefined())
        updatedUser->setEnabled(isEnabled.value());

    if (!data.password.isEmpty())
    {
        /* set new password */
        updatedUser->setPasswordAndGenerateHash(data.password);
    }
    else if (!data.passwordHash.isEmpty())
    {
        updatedUser->setRealm(data.realm);
        updatedUser->setHash(data.passwordHash);
        updatedUser->setDigest(data.passwordDigest);
        if (!data.cryptSha512Hash.isEmpty())
            updatedUser->setCryptSha512Hash(data.cryptSha512Hash);
    }

    ec2::ApiUserData apiUser;
    fromResourceToApi(updatedUser, apiUser);

    if (apiOldUser == apiUser)
        return true; //< Nothing to update.

    auto errCode = connection->getUserManager(Qn::kSystemAccess)->saveSync(apiUser, data.password);
    NX_ASSERT(errCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errCode != ec2::ErrorCode::ok)
    {
        if (errString)
            *errString = lit("Internal server database error: %1").arg(toString(errCode));
        return false;
    }
    updatedUser->resetPassword();

    HostSystemPasswordSynchronizer::instance()->syncLocalHostRootPasswordWithAdminIfNeeded(updatedUser);
    return true;
}

bool validatePasswordData(const PasswordData& passwordData, QString* errStr)
{
    if (errStr)
        errStr->clear();

    if (!(passwordData.passwordHash.isEmpty() == passwordData.realm.isEmpty() &&
        passwordData.passwordDigest.isEmpty() == passwordData.realm.isEmpty() &&
        passwordData.cryptSha512Hash.isEmpty() == passwordData.realm.isEmpty()))
    {
        //these values MUST be all filled or all NOT filled
        NX_LOG(lit("All password hashes MUST be supplied all together along with realm"), cl_logDEBUG2);

        if (errStr)
            *errStr = lit("All password hashes MUST be supplied all together along with realm");
        return false;
    }

    return true;
}

bool backupDatabase(std::shared_ptr<ec2::AbstractECConnection> connection)
{
    const nx::utils::SoftwareVersion productVersion(nx::utils::AppInfo::applicationVersion());
    QString buildSuffix = lit("_v") + QString::number(productVersion.build());

    QString dir = getDataDirectory() + lit("/");
    QString fileName;
    for (int i = -1; ; ++i)
    {
        QString numSuffix = (i < 0) ? QString() : lit("_") + QString::number(i);
        fileName = dir + lit("ecs") + buildSuffix + numSuffix + lit(".backup");
        if (!QFile::exists(fileName))
            break;
    }

    const ec2::ErrorCode errorCode = connection->dumpDatabaseToFileSync(fileName);
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("Failed to dump EC database: %1").arg(ec2::toString(errorCode)), cl_logERROR);
        return false;
    }
    return true;
}

void dropConnectionsToRemotePeers(ec2::AbstractTransactionMessageBus* messageBus)
{
    if (QnServerConnector::instance())
        QnServerConnector::instance()->stop();

    messageBus->dropConnections();
}

void resumeConnectionsToRemotePeers()
{
    if (QnServerConnector::instance())
        QnServerConnector::instance()->start();
}

bool changeLocalSystemId(const ConfigureSystemData& data, ec2::AbstractTransactionMessageBus* messageBus)
{
    const auto& commonModule = messageBus->commonModule();
    if (commonModule->globalSettings()->localSystemId() == data.localSystemId)
        return true;

    QnMediaServerResourcePtr server = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(commonModule->moduleGUID());
    if (!server) {
        NX_LOG("Cannot find self server resource!", cl_logERROR);
        return false;
    }

    if (!data.wholeSystem)
        dropConnectionsToRemotePeers(messageBus);

    auto connection = commonModule->ec2Connection();

    // add foreign users
    for (const auto& user: data.foreignUsers)
    {
        if (connection->getUserManager(Qn::kSystemAccess)->saveSync(user) != ec2::ErrorCode::ok)
        {
            if (!data.wholeSystem)
                resumeConnectionsToRemotePeers();
            return false;
        }
    }

    // add foreign resource params
    if (connection->getResourceManager(Qn::kSystemAccess)->saveSync(data.additionParams) != ec2::ErrorCode::ok)
    {
        if (!data.wholeSystem)
            resumeConnectionsToRemotePeers();
        return false;
    }

    // apply remove settings
    const auto& settings = commonModule->globalSettings()->allSettings();
    for(const auto& foreignSetting: data.foreignSettings)
    {
        for(QnAbstractResourcePropertyAdaptor* setting: settings)
        {
            if (setting->key() == foreignSetting.name)
            {
                setting->setSerializedValue(foreignSetting.value);
                break;
            }
        }
    }

    commonModule->setSystemIdentityTime(data.sysIdTime, commonModule->moduleGUID());

    if (data.localSystemId.isNull())
        commonModule->globalSettings()->resetCloudParams();

    if (!data.systemName.isEmpty())
        commonModule->globalSettings()->setSystemName(data.systemName);

    commonModule->globalSettings()->setLocalSystemId(data.localSystemId);
    commonModule->globalSettings()->synchronizeNowSync();

    commonModule->ec2Connection()->setTransactionLogTime(data.tranLogTime);

    // update auth key if system name is changed
    server->setAuthKey(QnUuid::createUuid().toString());
    nx::ServerSetting::setAuthKey(server->getAuthKey().toLatin1());
    ec2::ApiMediaServerData apiServer;
    fromResourceToApi(server, apiServer);
    if (connection->getMediaServerManager(Qn::kSystemAccess)->saveSync(apiServer) != ec2::ErrorCode::ok)
    {
        NX_LOG("Failed to update server auth key while configuring system", cl_logWARNING);
    }

    if (!data.foreignServer.id.isNull())
    {
        // add foreign server to pass auth if admin user is disabled
        if (connection->getMediaServerManager(Qn::kSystemAccess)->saveSync(data.foreignServer) != ec2::ErrorCode::ok)
        {
            NX_LOG("Failed to add foreign server while configuring system", cl_logWARNING);
        }
    }

    if (!data.wholeSystem)
        resumeConnectionsToRemotePeers();

    return true;
}

bool resetSystemToStateNew(QnCommonModule* commonModule)
{
    NX_LOG(lit("Resetting system to the \"new\" state"), cl_logINFO);

    commonModule->globalSettings()->setLocalSystemId(QnUuid());   //< Marking system as a "new".
    if (!commonModule->globalSettings()->synchronizeNowSync())
    {
        NX_LOG(lit("Error saving changes to global settings"), cl_logINFO);
        return false;
    }

    auto adminUserResource = commonModule->resourcePool()->getAdministrator();
    PasswordData data;
    data.password = helpers::kFactorySystemPassword;
    return updateUserCredentials(commonModule->ec2Connection(), data, QnOptionalBool(true), adminUserResource, nullptr);
}

// -------------- nx::ServerSetting -----------------------

qint64 nx::ServerSetting::getSysIdTime()
{
    return qnServerModule->roSettings()->value(SYSTEM_IDENTITY_TIME).toLongLong();
}

void nx::ServerSetting::setSysIdTime(qint64 value)
{
    auto settings = qnServerModule->roSettings();
    settings->setValue(SYSTEM_IDENTITY_TIME, value);
}

QByteArray nx::ServerSetting::decodeAuthKey(const QByteArray& authKey)
{
    // convert from v2.2 format and encode value
    QByteArray prefix("SK_");
    if (authKey.startsWith(prefix)) {
        QByteArray authKeyEncoded = QByteArray::fromHex(authKey.mid(prefix.length()));
        QByteArray authKeyDecoded = nx::utils::encodeSimple(authKeyEncoded);
        return QnUuid::fromRfc4122(authKeyDecoded).toByteArray();
    }
    else {
        return authKey;
    }
}

QByteArray nx::ServerSetting::getAuthKey()
{
    QByteArray authKey = qnServerModule->roSettings()->value(AUTH_KEY).toByteArray();
    QString appserverHostString = qnServerModule->roSettings()->value("appserverHost").toString();
    if (!authKey.isEmpty())
    {
        // convert from v2.2 format and encode value
        QByteArray prefix("SK_");
        if (!authKey.startsWith(prefix))
            setAuthKey(authKey);
        else
            authKey = decodeAuthKey(authKey);
    }
    return authKey;
}

void nx::ServerSetting::setAuthKey(const QByteArray& authKey)
{
    QByteArray prefix("SK_");
    QByteArray authKeyBin = QnUuid(authKey).toRfc4122();
    QByteArray authKeyEncoded = nx::utils::encodeSimple(authKeyBin).toHex();
    qnServerModule->roSettings()->setValue(AUTH_KEY, prefix + authKeyEncoded); // encode and update in settings
}


// -------------- nx::SystemName -----------------------

nx::SystemName::SystemName(const SystemName& other):
    m_value(other.m_value)
{
}

nx::SystemName::SystemName(const QString& value)
{
    m_value = value;
}

QString nx::SystemName::value() const
{
    if (m_value.startsWith(AUTO_GEN_SYSTEM_NAME))
        return m_value.mid(AUTO_GEN_SYSTEM_NAME.length());
    else
        return m_value;
}

QString nx::SystemName::prevValue() const
{
    if (m_prevValue.startsWith(AUTO_GEN_SYSTEM_NAME))
        return m_prevValue.mid(AUTO_GEN_SYSTEM_NAME.length());
    else
        return m_prevValue;
}

bool nx::SystemName::saveToConfig()
{
    const auto prevValueBak = m_prevValue;
    m_prevValue = m_value;
    auto settings = qnServerModule->roSettings();
    if (!settings->isWritable())
    {
        m_prevValue = prevValueBak;
        return false;
    }
    settings->setValue(SYSTEM_NAME_KEY, m_value);
    settings->setValue(PREV_SYSTEM_NAME_KEY, m_prevValue);
    return true;
}

void nx::SystemName::loadFromConfig()
{
    auto settings = qnServerModule->roSettings();
    m_value = settings->value(SYSTEM_NAME_KEY).toString();
    m_prevValue = settings->value(PREV_SYSTEM_NAME_KEY).toString();
}

void nx::SystemName::resetToDefault()
{
    m_value = QString(lit("%1system_%2")).arg(QString::fromLatin1(AUTO_GEN_SYSTEM_NAME)).arg(getMacFromPrimaryIF());
}

bool nx::SystemName::isDefault() const
{
    return m_value.startsWith(AUTO_GEN_SYSTEM_NAME);
}

void nx::SystemName::clear()
{
    m_value.clear();
    m_prevValue.clear();
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
