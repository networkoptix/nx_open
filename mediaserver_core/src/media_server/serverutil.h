#ifndef _SERVER_UTIL_H
#define _SERVER_UTIL_H

#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/fusion/fusion_fwd.h>

// TODO: #Elric this belongs together with server_settings

QByteArray decodeAuthKey(const QByteArray&);
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

        void saveToConfig();
        void loadFromConfig();
    private:
        QString m_value;
        QString m_prevValue;
    };
}

/*
* @param systemName - new system name
* @param sysIdTime - database recovery time (last back time)
* @param tranLogTime - move transaction time to position at least tranLogTime
*/
bool changeSystemName(nx::SystemName systemName, qint64 sysIdTime, qint64 tranLogTime);


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
    QString oldPassword;
};

#define PasswordData_Fields (password)(realm)(passwordHash)(passwordDigest)(cryptSha512Hash)(oldPassword)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (PasswordData),
    (json));


bool changeAdminPassword(PasswordData data);
bool validatePasswordData(const PasswordData& passwordData, QString* errStr);


bool isLocalAppServer(const QString &host);

qint64 getSysIdTime();
void setSysIdTime(qint64 value);


#endif // _SERVER_UTIL_H
