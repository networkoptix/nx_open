#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <nx/fusion/model_functions_fwd.h>
#include <core/resource_access/user_access_data.h>
#include <utils/common/optional.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/timestamp.h>
#include <api/model/configure_system_data.h>
#include <nx/vms/server/server_module_aware.h>

class QnCommonModule;

namespace ec2 {
    class AbstractTransactionMessageBus;
    class AbstractECConnection;
}

namespace nx {
namespace vms::server {

// Config based system name
class SystemName: public ServerModuleAware
{
public:
    SystemName(QnMediaServerModule* serverModule, const QString& value = QString());
    SystemName(const SystemName& other);

    QString value() const;
    QString prevValue() const;

    void resetToDefault();
    bool isDefault() const;
    void clear();

    bool saveToConfig();
    void loadFromConfig();
private:
    QString m_value;
    QString m_prevValue;
};

class SettingsHelper: public ServerModuleAware
{
public:
    SettingsHelper(QnMediaServerModule* serverModule);

    /** Encode and save server auth key to the config/registry */
    void setAuthKey(const QByteArray& authKey);

    /** Read and decode server auth key from the config/registry */
    QByteArray getAuthKey();

    qint64 getSysIdTime();
    void setSysIdTime(qint64 value);
private:
    static QByteArray decodeAuthKey(const QByteArray& authKey);
};

class Utils: public ServerModuleAware
{
public:
    Utils(QnMediaServerModule* serverModule);

    bool updateUserCredentials(
        PasswordData data,
        QnOptionalBool isEnabled,
        const QnUserResourcePtr& userRes,
        QString* errString = nullptr);

    static bool isLocalAppServer(const QString& host);

    /*
    * @param localSystemId - new local system id
    * @param sysIdTime - database recovery time (last back time)
    * @param tranLogTime - move transaction time to position at least tranLogTime
    */
    bool configureLocalSystem(const ConfigureSystemData& data);

    /**
     * Auto detect HTTP content type based on message body
     */
    static QByteArray autoDetectHttpContentType(const QByteArray& msgBody);

    /**
     * Drop message bus connections to other servers in the system.
     */
    static void dropConnectionsToRemotePeers(ec2::AbstractTransactionMessageBus* messageBus);

    /**
     * Resume connection listening
     */
    static void resumeConnectionsToRemotePeers(ec2::AbstractTransactionMessageBus* messageBus);

    void syncStoragesToSettings(const QnMediaServerResourcePtr& server);

    bool backupDatabase(
        const boost::optional<QString>& dbFilePath = boost::none,
        const boost::optional<int>& buildNumber = boost::none);
    /**
     * Returns none if no backup file has been found.
     */
    boost::optional<int64_t> lastDbBackupTimestamp() const;
    bool timeToMakeDbBackup() const;
};

} // namespace vms::server
} // namespace nx

