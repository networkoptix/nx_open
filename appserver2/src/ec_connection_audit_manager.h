/**********************************************************
* 28 may 2014
* a.kolesnikov
***********************************************************/

#pragma once

#include "nx_ec/ec_api.h"

#include "transaction/transaction.h"
#include <transaction/transaction_log.h>
#include "api/model/audit/auth_session.h"

namespace ec2
{

class AbstractECConnection;

class ECConnectionAuditManager : public QObject
{
    Q_OBJECT
public:
    ECConnectionAuditManager(AbstractECConnection* ecConnection);
    virtual ~ECConnectionAuditManager() override;

    template <class T>
    void addAuditRecord(ApiCommand::Value command, const T& params, const QnAuthSession& authInfo)
    {
        // nothing to do by default
        Q_UNUSED(command);
        Q_UNUSED(params);
        Q_UNUSED(authInfo);
    }

    void addAuditRecord(ApiCommand::Value command,  const ApiCameraAttributesData& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiCameraAttributesDataList& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiMediaServerUserAttributesData& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiMediaServerUserAttributesDataList& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiStorageData& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiStorageDataList& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiUserData& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiUserDataList& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiBusinessRuleData& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiBusinessRuleDataList& params, const QnAuthSession& authInfo);

    void addAuditRecord(ApiCommand::Value command,  const ApiResourceParamWithRefData& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiResourceParamWithRefDataList& params, const QnAuthSession& authInfo);

    void addAuditRecord(ApiCommand::Value command,  const ApiIdData& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiIdDataList& params, const QnAuthSession& authInfo);

    void addAuditRecord(ApiCommand::Value command,  const ApiResetBusinessRuleData& params, const QnAuthSession& authInfo);
    void addAuditRecord(ApiCommand::Value command,  const ApiDatabaseDumpData& params, const QnAuthSession& authInfo);

    AbstractECConnection* ec2Connection() const;
private:
    void at_resourceAboutToRemoved(const QnUuid& id);
private:
    AbstractECConnection* m_connection;
    QMap<QnUuid, QString> m_remvedResourceNames;
};

}
