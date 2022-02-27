// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/uuid.h>

#include <nx/vms/auth/abstract_user_data_provider.h>

#include <common/common_module_aware.h>

class NX_VMS_COMMON_API GenericUserDataProvider:
    public QObject,
    public /*mixin*/ QnCommonModuleAware,
    public nx::vms::auth::AbstractUserDataProvider,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    GenericUserDataProvider(QnCommonModule* commonModule);
    ~GenericUserDataProvider();

    virtual QnResourcePtr findResByName(const nx::String& nxUserName) const override;

    virtual nx::vms::common::AuthResult authorize(
        const QnResourcePtr& res,
        const nx::network::http::Method& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) override;

    virtual std::tuple<nx::vms::common::AuthResult, QnResourcePtr> authorize(
        const nx::network::http::Method& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) override;

private:
    mutable nx::Mutex m_mutex;
    QMap<QnUuid, QnUserResourcePtr> m_users;
    QMap<QnUuid, QnMediaServerResourcePtr> m_servers;

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr& res);
    void at_resourcePool_resourceRemoved(const QnResourcePtr& res);
};
