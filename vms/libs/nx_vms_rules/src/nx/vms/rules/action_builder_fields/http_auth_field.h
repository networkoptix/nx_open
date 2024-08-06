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
    Q_CLASSINFO("encrypt", "password, token")

    Q_PROPERTY(nx::network::http::AuthType authType READ authType WRITE setAuthType NOTIFY
            authTypeChanged)
    Q_PROPERTY(QString login READ login WRITE setLogin NOTIFY loginChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(QString token READ token WRITE setToken NOTIFY tokenChanged)

public:
    using ActionBuilderField::ActionBuilderField;

    nx::network::http::AuthType authType() const;
    void setAuthType(const nx::network::http::AuthType& authType);

    QString login() const;
    void setLogin(const QString& login);

    QString password() const;
    void setPassword(const QString& password);

    QString token() const;
    void setToken(const QString& token);

    const AuthenticationInfo& auth() const;

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

signals:
    void authTypeChanged();
    void loginChanged();
    void passwordChanged();
    void tokenChanged();

private:
    AuthenticationInfo m_auth;
};

} // namespace nx::vms::rules
