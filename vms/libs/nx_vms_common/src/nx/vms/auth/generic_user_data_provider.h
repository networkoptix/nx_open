// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_module_aware.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/uuid.h>
#include <nx/vms/auth/abstract_user_data_provider.h>

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

    virtual std::pair<QnResourcePtr, bool /*hasClash*/> findResByName(
        const nx::String& nxUserName) const override;

    virtual nx::network::rest::AuthResult authorize(
        const QnResourcePtr& res,
        const nx::network::http::Method& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) override;

    virtual std::tuple<nx::network::rest::AuthResult, QnResourcePtr> authorize(
        const nx::network::http::Method& method,
        const nx::network::http::header::Authorization& authorizationHeader,
        nx::network::http::HttpHeaders* const responseHeaders) override;


};
