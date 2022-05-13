// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/vms/utils/system_uri.h>
#include <nx/utils/url.h>

class NX_VMS_CLIENT_CORE_API QnCloudUrlHelper: public QObject
{
    Q_OBJECT

public:
    QnCloudUrlHelper(
        nx::vms::utils::SystemUri::ReferralSource source,
        nx::vms::utils::SystemUri::ReferralContext context,
        QObject* parent = nullptr);

    Q_INVOKABLE QUrl mainUrl() const;
    Q_INVOKABLE QUrl aboutUrl() const;
    Q_INVOKABLE QUrl accountManagementUrl() const;
    Q_INVOKABLE QUrl accountSecurityUrl() const;
    Q_INVOKABLE QUrl createAccountUrl() const;
    Q_INVOKABLE QUrl restorePasswordUrl() const;
    Q_INVOKABLE QUrl faqUrl() const;
    Q_INVOKABLE QUrl viewSystemUrl() const;
    Q_INVOKABLE QUrl cloudLinkUrl(bool withReferral) const;

private:
    QUrl makeUrl(const QString& path = QString(), bool withReferral = true) const;

public:
    nx::vms::utils::SystemUri::ReferralSource m_source;
    nx::vms::utils::SystemUri::ReferralContext m_context;
};
