// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/network/http/http_types.h>
#include <nx/vms/rules/field_types.h>

#include "../action_builder_field.h"

namespace nx::vms::rules {

/*
 * Displayed as ComboBox with input fields.
 * View of input fields depends on combobox value
 */
class NX_VMS_RULES_API HttpAuthField: public ActionBuilderField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.httpAuth")

    Q_PROPERTY(nx::network::http::AuthType authType READ authType WRITE setAuthType NOTIFY
            authTypeChanged)
    Q_PROPERTY(std::string login READ login WRITE setLogin NOTIFY loginChanged)
    Q_PROPERTY(std::string password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(std::string token READ token WRITE setToken NOTIFY tokenChanged)
    Q_PROPERTY(AuthenticationInfo auth READ auth WRITE setAuth NOTIFY authChanged)


public:
    HttpAuthField() = default;

    nx::network::http::AuthType authType() const;
    void setAuthType(const nx::network::http::AuthType& authType);

    std::string login() const;
    void setLogin(const std::string& login);

    std::string password() const;
    void setPassword(const std::string& password);

    std::string token() const;
    void setToken(const std::string& token);

    AuthenticationInfo auth() const;
    void setAuth(const AuthenticationInfo& auth);

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

signals:
    void authTypeChanged();
    void loginChanged();
    void passwordChanged();
    void tokenChanged();
    void authChanged();

private:
    AuthenticationInfo m_auth;
};

} // namespace nx::vms::rules
