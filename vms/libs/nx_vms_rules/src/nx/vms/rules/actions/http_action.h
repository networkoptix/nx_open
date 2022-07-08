// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API HttpAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.http")

    FIELD(std::chrono::microseconds, interval, setInterval)
    Q_PROPERTY(QString url READ url WRITE setUrl)
    Q_PROPERTY(QString content READ content WRITE setContent)
    FIELD(QString, contentType, setContentType)
    Q_PROPERTY(QString login READ login WRITE setLogin)
    Q_PROPERTY(QString password READ password WRITE setPassword)
    FIELD(QString, method, setMethod)
    // TODO: #amalov Decide what to do with auth type enum.

public:
    static const ItemDescriptor& manifest();

    QString url() const;
    void setUrl(const QString& url);

    QString content() const;
    void setContent(const QString& content);

    QString login() const;
    void setLogin(const QString& login);

    QString password() const;
    void setPassword(const QString& password);

private:
    QString m_url;
    QString m_content;
    QString m_login;
    QString m_password;
};

} // namespace nx::vms::rules
