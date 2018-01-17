#pragma once
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <nx/fusion/model_functions_fwd.h>
//#include <nx/fusion/fusion/fusion_fwd.h>
#include <core/resource_access/user_access_data.h>
#include <utils/common/optional.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/transaction_timestamp.h>
#include <api/model/configure_system_data.h>

// TODO: #Elric this belongs together with server_settings

QnUuid serverGuid();
void setUseAlternativeGuid(bool value);

class QnCommonModule;

namespace ec2 {
    class AbstractTransactionMessageBus;
    class AbstractECConnection;
}

QString getDataDirectory();
void syncStoragesToSettings(const QnMediaServerResourcePtr &server);
QString makeNextUniqueName(const QString& dir, int build);
bool backupDatabase(std::shared_ptr<ec2::AbstractECConnection> connection);

namespace nx
{
    // Config based system name
    class SystemName
    {
    public:
        SystemName(const QString& value = QString());
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

    class ServerSetting
    {
    public:

        /** Encode and save server auth key to the config/registry */
        static void setAuthKey(const QByteArray& authKey);

        /** Read and decode server auth key from the config/registry */
        static QByteArray getAuthKey();

        static qint64 getSysIdTime();
        static void setSysIdTime(qint64 value);
    private:
        static QByteArray decodeAuthKey(const QByteArray& authKey);
    };
}


void resetTransactionTransportConnections();

bool updateUserCredentials(
    std::shared_ptr<ec2::AbstractECConnection> connection,
    PasswordData data,
    QnOptionalBool isEnabled,
    const QnUserResourcePtr& userRes,
    QString* errString = nullptr);
bool validatePasswordData(const PasswordData& passwordData, QString* errStr);


bool isLocalAppServer(const QString &host);


/*
* @param localSystemId - new local system id
* @param sysIdTime - database recovery time (last back time)
* @param tranLogTime - move transaction time to position at least tranLogTime
*/
bool changeLocalSystemId(
    const ConfigureSystemData& data,
    ec2::AbstractTransactionMessageBus* messageBus);

/**
 * Auto detect HTTP content type based on message body
 */
QByteArray autoDetectHttpContentType(const QByteArray& msgBody);

/**
 * @return false if failed to save some data.
 */
bool resetSystemToStateNew(QnCommonModule* commonModule);

/**
 * Drop message bus connections to other servers in the system.
 */
void dropConnectionsToRemotePeers(ec2::AbstractTransactionMessageBus* messageBus);

/**
 * Resume connection listening
 */
void resumeConnectionsToRemotePeers();
