// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_action.h>

namespace nx::vms::rules {

class NX_VMS_RULES_API HttpAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.http")

public:
    enum class ContentType
    {
        Auto,
        TextPlain,
        TextHtml,
        ApplicationHtml,
        ApplicationJson,
        ApplicationXml
    };
    Q_ENUM(ContentType)

    Q_PROPERTY(QString url READ url WRITE setUrl)
    Q_PROPERTY(QString content READ content WRITE setContent)
    Q_PROPERTY(ContentType contentType READ contentType WRITE setContentType)
    Q_PROPERTY(QString login READ login WRITE setLogin)
    Q_PROPERTY(QString password READ password WRITE setPassword)

public:
    QString url() const;
    void setUrl(const QString& url);

    QString content() const;
    void setContent(const QString& content);

    ContentType contentType() const;
    void setContentType(ContentType contentType);

    QString login() const;
    void setLogin(const QString& login);

    QString password() const;
    void setPassword(const QString& password);

private:
    QString m_url;
    QString m_content;
    ContentType m_contentType;
    QString m_login;
    QString m_password;
};

} // namespace nx::vms::rules