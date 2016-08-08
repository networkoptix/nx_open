#ifndef _SERVER_UTIL_H
#define _SERVER_UTIL_H

#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <core/resource_management/user_access_data.h>
#include <utils/common/optional.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/data/api_user_data.h>

// TODO: #Elric this belongs together with server_settings

QnUuid serverGuid();
void setUseAlternativeGuid(bool value);

QString getDataDirectory();
void syncStoragesToSettings(const QnMediaServerResourcePtr &server);
bool backupDatabase();


namespace nx
{
    class SystemName
    {
    public:
        SystemName(const QString& value = QString());
        SystemName(const SystemName& other);

        QString value() const;
        QString prevValue() const;

        void resetToDefault();
        bool isDefault() const;

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


struct PasswordData
{
    PasswordData() {}
    PasswordData(const QnRequestParams& params);

    bool hasPassword() const;

    QString password;
    QByteArray realm;
    QByteArray passwordHash;
    QByteArray passwordDigest;
    QByteArray cryptSha512Hash;
};

#define PasswordData_Fields (password)(realm)(passwordHash)(passwordDigest)(cryptSha512Hash)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (PasswordData),
    (json));

bool updateUserCredentials(PasswordData data, QnOptionalBool isEnabled, const QnUserResourcePtr& userRes, QString* errString = nullptr);
bool validatePasswordData(const PasswordData& passwordData, QString* errStr);


bool isLocalAppServer(const QString &host);


struct ConfigureSystemData : public PasswordData
{
    ConfigureSystemData():
        PasswordData(),
        wholeSystem(false),
        sysIdTime(0),
        tranLogTime(0),
        port(0)
    {
    }

    ConfigureSystemData(const QnRequestParams& params) :
        PasswordData(params),
        systemName(params.value(lit("systemName"))),
        wholeSystem(params.value(lit("wholeSystem"), lit("false")) != lit("false")),
        sysIdTime(params.value(lit("sysIdTime")).toLongLong()),
        tranLogTime(params.value(lit("tranLogTime")).toLongLong()),
        port(params.value(lit("port")).toInt())
    {
    }

    QString systemName;
    bool wholeSystem;
    qint64 sysIdTime;
    qint64 tranLogTime;
    int port;
    ec2::ApiMediaServerData foreignServer;
    ec2::ApiUserData foreignUser;
    ec2::ApiResourceParamDataList foreignSettings;
};

/*
* @param systemName - new system name
* @param sysIdTime - database recovery time (last back time)
* @param tranLogTime - move transaction time to position at least tranLogTime
*/
bool changeSystemName(const ConfigureSystemData& data);


#define ConfigureSystemData_Fields PasswordData_Fields (systemName)(wholeSystem)(sysIdTime)(tranLogTime)(port)(foreignServer)(foreignUser)(foreignSettings)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ConfigureSystemData),
    (json));


#endif // _SERVER_UTIL_H
