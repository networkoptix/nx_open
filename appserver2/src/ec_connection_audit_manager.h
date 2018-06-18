#pragma once

#include "nx_ec/ec_api.h"

#include "transaction/transaction.h"
#include "api/model/audit/auth_session.h"

namespace ec2 {

class AbstractECConnection;

class ECConnectionAuditManager
{
public:
    ECConnectionAuditManager(AbstractECConnection* ecConnection);

    template<class T>
    void addAuditRecord(
        ApiCommand::Value /*command*/,
        const T& /*params*/,
        const QnAuthSession& /*authInfo*/)
    {
        // nothing to do by default
    }

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::CameraAttributesData& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::CameraAttributesDataList& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::MediaServerUserAttributesData& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::MediaServerUserAttributesDataList& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::StorageData& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::StorageDataList& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const ApiUserData& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const ApiUserDataList& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::EventRuleData& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::EventRuleDataList& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::ResourceParamWithRefData& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::ResourceParamWithRefDataList& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::IdData& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::IdDataList& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::ResetEventRulesData& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::DatabaseDumpData& params,
        const QnAuthSession& authInfo);

    AbstractECConnection* ec2Connection() const;
private:
    AbstractECConnection* m_connection;
};

} // namespace ec2

