// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QString>
#include <string>

#include <nx/network/http/http_types.h>

#include "../basic_action.h"
#include "../data_macros.h"
#include "../field_types.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API HttpAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.http")

    FIELD(std::chrono::microseconds, interval, setInterval)
    Q_PROPERTY(QString url READ url WRITE setUrl)
    Q_PROPERTY(QString content READ content WRITE setContent)
    FIELD(QString, contentType, setContentType)
    FIELD(QString, method, setMethod)
    Q_PROPERTY(AuthenticationInfo auth READ auth WRITE setAuth)

public:
    static const ItemDescriptor& manifest();

    QString url() const;
    void setUrl(const QString& url);

    QString content() const;
    void setContent(const QString& content);

    std::string login() const;
    std::string password() const;
    std::string token() const;

    AuthenticationInfo auth() const;
    void setAuth(const AuthenticationInfo& auth);

    nx::network::http::AuthType authType() const;

    virtual QVariantMap details(common::SystemContext* context) const override;

private:
    AuthenticationInfo m_auth;
    QString m_url;
    QString m_content;
};

} // namespace nx::vms::rules
