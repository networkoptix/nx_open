#ifndef _SERVER_UTIL_H
#define _SERVER_UTIL_H

#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <core/resource_management/user_access_data.h>

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

/*
* @param systemName - new system name
* @param sysIdTime - database recovery time (last back time)
* @param tranLogTime - move transaction time to position at least tranLogTime
*/
bool changeSystemName(nx::SystemName systemName, qint64 sysIdTime, qint64 tranLogTime, bool resetConnections, const Qn::UserAccessData &userAccessData);

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


bool changeAdminPassword(PasswordData data, const QnUuid &userId, QString* errString = nullptr);
bool validatePasswordData(const PasswordData& passwordData, QString* errStr);


bool isLocalAppServer(const QString &host);


#endif // _SERVER_UTIL_H
