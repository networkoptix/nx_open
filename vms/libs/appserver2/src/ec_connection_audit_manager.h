// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_module_aware.h>
#include <nx_ec/abstract_ec_connection.h>

#include "transaction/transaction.h"
#include "api/model/audit/auth_session.h"

namespace ec2 {

class AbstractECConnection;

class ECConnectionAuditManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:
    ECConnectionAuditManager(AbstractECConnection* ecConnection, QnCommonModule* commonModule);
    virtual ~ECConnectionAuditManager() override;

        template <class T>
    void addAuditRecord(
        ApiCommand::Value /*command*/,
        const T& /*params*/,
        const QnAuthSession& /*authInfo*/)
    {
        // nothing to do by default
    }

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::CameraData& params,
        const QnAuthSession& authInfo);

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
        const nx::vms::api::UserData& params,
        const QnAuthSession& authInfo);

    void addAuditRecord(
        ApiCommand::Value command,
        const nx::vms::api::UserDataList& params,
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
        const nx::vms::api::ResourceParamWithRefData& param,
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
    void at_resourceAboutToRemoved(const QnUuid& id);

private:
    AbstractECConnection* m_connection;
    QMap<QnUuid, QString> m_removedResourceNames;
};

} // namespace ec2
