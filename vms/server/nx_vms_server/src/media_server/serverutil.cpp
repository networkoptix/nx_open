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

#include <nx/vms/utils/vms_utils.h>

#include <media_server/serverutil.h>
#include <media_server/settings.h>

#include <utils/common/app_info.h>
#include <common/common_module.h>

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
#include <nx/utils/scope_guard.h>
#include <api/resource_property_adaptor.h>

#include <QtCore/QJsonDocument>
#include <QtXmlPatterns/QXmlSchema>
#include <QtXmlPatterns/QXmlSchemaValidator>
#include <QtXml/QDomDocument>

#include "server_connector.h"
#include "server/server_globals.h"
#include <media_server/media_server_module.h>
#include <utils/crypt/symmetrical.h>
#include <utils/common/synctime.h>
#include <api/model/password_data.h>
#include <database/db_manager.h>

#include <nx/vms/utils/system_helpers.h>

namespace
{
    static const QByteArray SYSTEM_NAME_KEY("systemName");
    static const QByteArray PREV_SYSTEM_NAME_KEY("prevSystemName");
    static const QByteArray AUTO_GEN_SYSTEM_NAME("__auto__");
    static const QByteArray SYSTEM_IDENTITY_TIME("sysIdTime");
    static const QByteArray AUTH_KEY("authKey");
}

static QnMediaServerResourcePtr m_server;

namespace nx {
namespace vms::server {

bool Utils::isLocalAppServer(const QString &host)
{
    return host.isEmpty() || host == "localhost" || host == "127.0.0.1" || QUrl(host).scheme() == "file";
}

Utils::Utils(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

bool Utils::updateUserCredentials(
    PasswordData data,
    QnOptionalBool isEnabled,
    const QnUserResourcePtr& userRes,
    QString* errString)
{
    QnUserResourcePtr updatedUser;
    auto connection = serverModule()->ec2Connection();
    if (!nx::vms::utils::updateUserCredentials(
            connection,
            std::move(data),
            isEnabled,
            userRes,
            errString,
            &updatedUser))
    {
        return false;
    }

    // TODO: #ak Test user for being owner or local admin?
    serverModule()->hostSystemPasswordSynchronizer()
        ->syncLocalHostRootPasswordWithAdminIfNeeded(updatedUser);
    return true;
}

bool Utils::backupDatabase(const boost::optional<QString>& dbFilePath,
    const boost::optional<int>& buildNumber)
{
    auto connection = ec2Connection();
    return nx::vms::utils::backupDatabase(serverModule()->settings().backupDir(),
        std::move(connection), dbFilePath, buildNumber);
}

bool Utils::timeToMakeDbBackup() const
{
    const auto dataDir = serverModule()->settings().dataDir();
    const auto backupDir = serverModule()->settings().backupDir();
    if (!QFile::exists(ec2::detail::QnDbManager::ecsDbFileName(dataDir)))
        return false; //< Nothing to make a copy of.

    const auto currentVersion =
        nx::utils::SoftwareVersion(nx::utils::AppInfo::applicationVersion());

    const auto allBackupFilesData = vms::utils::allBackupFilesDataSorted(backupDir);
    QList<nx::vms::utils::DbBackupFileData> thisVersionBackupFilesData;

    std::copy_if(allBackupFilesData.cbegin(), allBackupFilesData.cend(),
        std::back_inserter(thisVersionBackupFilesData),
        [&currentVersion](const nx::vms::utils::DbBackupFileData& backupFileData)
        {
            return backupFileData.build == currentVersion.build();
        });

    if (thisVersionBackupFilesData.isEmpty())
        return true; //< Backups for the current server version haven't been found, let's make one.

    NX_ASSERT(!thisVersionBackupFilesData.isEmpty());
    if (qnSyncTime->currentMSecsSinceEpoch() - thisVersionBackupFilesData.front().timestamp
        > serverModule()->settings().dbBackupPeriodMS().count())
    {
        return true;
    }

    return false;
}

boost::optional<int64_t> Utils::lastDbBackupTimestamp() const
{
    const auto backupDir = serverModule()->settings().backupDir();
    const auto allBackupFilesData = vms::utils::allBackupFilesDataSorted(backupDir);

    if (allBackupFilesData.isEmpty())
        return boost::none;

    return allBackupFilesData.front().timestamp;
}

void Utils::dropConnectionsToRemotePeers(ec2::AbstractTransactionMessageBus* messageBus)
{
    messageBus->commonModule()->setStandAloneMode(true);
    messageBus->dropConnections();
}

void Utils::resumeConnectionsToRemotePeers(ec2::AbstractTransactionMessageBus* messageBus)
{
    messageBus->commonModule()->setStandAloneMode(false);
}

bool Utils::configureLocalSystem(
    const ConfigureSystemData& data)
{
    auto messageBus = serverModule()->ec2Connection()->messageBus();

    // Duplicating localSystemId check so that connection are not dropped
    // in case if this method has nothing to do.
    const auto& commonModule = serverModule()->commonModule();
    if (commonModule->globalSettings()->localSystemId() == data.localSystemId)
        return true;

    nx::utils::Guard guard;
    if (!data.wholeSystem)
    {
        dropConnectionsToRemotePeers(messageBus);
        guard = nx::utils::Guard(
            [&data, messageBus]() { resumeConnectionsToRemotePeers(messageBus); });
    }

    if (!nx::vms::utils::configureLocalPeerAsPartOfASystem(commonModule, data))
        return false;

    QnMediaServerResourcePtr server =
        commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
            commonModule->moduleGUID());
    NX_ASSERT(server);
    if (!server)
        return false;

    SettingsHelper(serverModule()).setAuthKey(server->getAuthKey().toLatin1());
    return true;
}

// -------------- nx::ServerSetting -----------------------

SettingsHelper::SettingsHelper(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

qint64 SettingsHelper::getSysIdTime()
{
    return serverModule()->settings().sysIdTime();
}

void SettingsHelper::setSysIdTime(qint64 value)
{
    serverModule()->mutableSettings()->sysIdTime.set(value);
}

QByteArray SettingsHelper::decodeAuthKey(const QByteArray& authKey)
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

QByteArray SettingsHelper::getAuthKey()
{
    QByteArray authKey = serverModule()->settings().authKey();
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

void SettingsHelper::setAuthKey(const QByteArray& authKey)
{
    QByteArray prefix("SK_");
    QByteArray authKeyBin = QnUuid(authKey).toRfc4122();
    QByteArray authKeyEncoded = nx::utils::encodeSimple(authKeyBin).toHex();

    // Encode and update in settings.
    serverModule()->mutableSettings()->authKey.set(prefix + authKeyEncoded);
}

// -------------- SystemName -----------------------

SystemName::SystemName(const SystemName& other):
    ServerModuleAware(other.serverModule()),
    m_value(other.m_value)
{
}

SystemName::SystemName(QnMediaServerModule* serverModule, const QString& value):
    ServerModuleAware(serverModule),
    m_value(value)
{
}

QString SystemName::value() const
{
    if (m_value.startsWith(AUTO_GEN_SYSTEM_NAME))
        return m_value.mid(AUTO_GEN_SYSTEM_NAME.length());
    else
        return m_value;
}

QString SystemName::prevValue() const
{
    if (m_prevValue.startsWith(AUTO_GEN_SYSTEM_NAME))
        return m_prevValue.mid(AUTO_GEN_SYSTEM_NAME.length());
    else
        return m_prevValue;
}

bool SystemName::saveToConfig()
{
    const auto prevValueBak = m_prevValue;
    m_prevValue = m_value;
    auto settings = serverModule()->roSettings();
    if (!settings->isWritable())
    {
        m_prevValue = prevValueBak;
        return false;
    }
    settings->setValue(SYSTEM_NAME_KEY, m_value);
    settings->setValue(PREV_SYSTEM_NAME_KEY, m_prevValue);
    return true;
}

void SystemName::loadFromConfig()
{
    auto settings = serverModule()->roSettings();
    m_value = settings->value(SYSTEM_NAME_KEY).toString();
    m_prevValue = settings->value(PREV_SYSTEM_NAME_KEY).toString();
}

void SystemName::resetToDefault()
{
    m_value = QString(lit("%1system_%2")).arg(QString::fromLatin1(AUTO_GEN_SYSTEM_NAME)).arg(nx::network::getMacFromPrimaryIF());
}

bool SystemName::isDefault() const
{
    return m_value.startsWith(AUTO_GEN_SYSTEM_NAME);
}

void SystemName::clear()
{
    m_value.clear();
    m_prevValue.clear();
}

QByteArray Utils::autoDetectHttpContentType(const QByteArray& msgBody)
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

} // namespace vms::server
} // namespace nx
