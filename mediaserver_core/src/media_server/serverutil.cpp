#include <QtCore/QString>
#include <nx/utils/uuid.h>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>

#include <nx_ec/managers/abstract_server_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/dummy_handler.h>

#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <nx/utils/log/log.h>
#include <utils/common/app_info.h>
#include <common/common_module.h>
#include <network/authenticate_helper.h>
#include <nx/network/nettools.h>

#include <utils/crypt/linux_passwd_crypt.h>
#include <core/resource/user_resource.h>
#include <server/host_system_password_synchronizer.h>


namespace
{
    static const QByteArray SYSTEM_NAME_KEY("systemName");
    static const QByteArray PREV_SYSTEM_NAME_KEY("prevSystemName");
    static const QByteArray AUTO_GEN_SYSTEM_NAME("__auto__");
    static const QByteArray SYSTEM_IDENTITY_TIME("sysIdTime");
}

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

QnUuid serverGuid()
{
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

PasswordData::PasswordData(const QnRequestParams &params)
{
    password = params.value(lit("password"));
    oldPassword = params.value(lit("oldPassword"));
    realm = params.value(lit("realm")).toLatin1();
    passwordHash = params.value(lit("passwordHash")).toLatin1();
    passwordDigest = params.value(lit("passwordDigest")).toLatin1();
    cryptSha512Hash = params.value(lit("cryptSha512Hash")).toLatin1();
}

bool PasswordData::hasPassword() const
{
    return
        !password.isEmpty() ||
        !passwordHash.isEmpty() || 
        !passwordDigest.isEmpty();
}

bool changeAdminPassword(PasswordData data)
{
    //genereating cryptSha512Hash
    if (data.cryptSha512Hash.isEmpty() && !data.password.isEmpty())
        data.cryptSha512Hash = linuxCryptSha512(data.password.toUtf8(), generateSalt(LINUX_CRYPT_SALT_LENGTH));


    QnUserResourcePtr admin = qnResPool->getAdministrator();
    if (!admin)
        return false;

    //making copy of admin user to be able to rollback local changed on DB update failure
    QnUserResourcePtr updatedAdmin = QnUserResourcePtr(new QnUserResource(*admin));

    if (data.password.isEmpty() &&
        updatedAdmin->getHash() == data.passwordHash &&
        updatedAdmin->getDigest() == data.passwordDigest &&
        updatedAdmin->getCryptSha512Hash() == data.cryptSha512Hash)
    {
        //no need to update anything
        return true;
    }

    if (!data.password.isEmpty())
    {
        /* check old password */
        if (!admin->checkPassword(data.oldPassword))
            return false;

        /* set new password */
        updatedAdmin->setPassword(data.password);
        updatedAdmin->generateHash();
        QnUserResourceList updatedUsers;
        if (QnAppServerConnectionFactory::getConnection2()->getUserManager()->saveSync(updatedAdmin, &updatedUsers) != ec2::ErrorCode::ok)
            return false;
        updatedAdmin->setPassword(QString());
    }
    else {
        updatedAdmin->setRealm(data.realm);
        updatedAdmin->setHash(data.passwordHash);
        updatedAdmin->setDigest(data.passwordDigest);
        if (!data.cryptSha512Hash.isEmpty())
            updatedAdmin->setCryptSha512Hash(data.cryptSha512Hash);
        QnUserResourceList updatedUsers;
        if (QnAppServerConnectionFactory::getConnection2()->getUserManager()->saveSync(updatedAdmin, &updatedUsers) != ec2::ErrorCode::ok)
            return false;
    }

    //applying changes to local resource
    //TODO #ak following changes are done non-atomically
    admin->setRealm(updatedAdmin->getRealm());
    admin->setHash(updatedAdmin->getHash());
    admin->setDigest(updatedAdmin->getDigest());
    admin->setCryptSha512Hash(updatedAdmin->getCryptSha512Hash());

    HostSystemPasswordSynchronizer::instance()->syncLocalHostRootPasswordWithAdminIfNeeded(updatedAdmin);
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
    
    if (!passwordData.password.isEmpty() && passwordData.oldPassword.isEmpty())
    {
        //these values MUST be all filled or all NOT filled
        NX_LOG(lit("Old password MUST be provided"), cl_logDEBUG2);
        if (errStr)
            *errStr = lit("Old password MUST be provided");
        return false;
    }

    return true;
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

bool changeSystemName(nx::SystemName systemName, qint64 sysIdTime, qint64 tranLogTime)
{
    if (qnCommon->localSystemName() == systemName.value())
        return true;

    qnCommon->setLocalSystemName(systemName.value());
    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (!server) {
        NX_LOG("Cannot find self server resource!", cl_logERROR);
        return false;
    }

    systemName.saveToConfig();
    server->setSystemName(systemName.value());
    qnCommon->setSystemIdentityTime(sysIdTime, qnCommon->moduleGUID());
    if (systemName.isDefault())
        server->setServerFlags(server->getServerFlags() | Qn::SF_AutoSystemName);
    else
        server->setServerFlags(server->getServerFlags() & ~Qn::SF_AutoSystemName);
    QnAppServerConnectionFactory::getConnection2()->setTransactionLogTime(tranLogTime);

    ec2::ApiMediaServerData apiServer;
    fromResourceToApi(server, apiServer);
    QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->save(apiServer, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    return true;
}

qint64 getSysIdTime()
{
    return MSSettings::roSettings()->value(SYSTEM_IDENTITY_TIME).toLongLong();
}

void setSysIdTime(qint64 value)
{
    auto settings = MSSettings::roSettings();
    settings->setValue(SYSTEM_IDENTITY_TIME, QString());
}

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

void nx::SystemName::saveToConfig()
{
    m_prevValue = m_value;
    auto settings = MSSettings::roSettings();
    settings->setValue(SYSTEM_NAME_KEY, m_value);
    settings->setValue(PREV_SYSTEM_NAME_KEY, m_prevValue);
}

void nx::SystemName::loadFromConfig()
{
    auto settings = MSSettings::roSettings();
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
