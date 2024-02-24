// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/url.h>
#include <nx/vms/utils/system_uri.h>

namespace nx::vms::client::core {

class SystemContext;

class NX_VMS_CLIENT_CORE_API CloudUrlHelper: public QObject
{
    Q_OBJECT

public:
    CloudUrlHelper(
        utils::SystemUri::ReferralSource source,
        utils::SystemUri::ReferralContext context,
        QObject* parent = nullptr);

    Q_INVOKABLE QUrl mainUrl() const;
    Q_INVOKABLE QUrl aboutUrl() const;
    Q_INVOKABLE QUrl accountManagementUrl() const;
    Q_INVOKABLE QUrl accountSecurityUrl() const;
    Q_INVOKABLE QUrl createAccountUrl() const;
    Q_INVOKABLE QUrl restorePasswordUrl() const;
    Q_INVOKABLE QUrl faqUrl() const;
    Q_INVOKABLE QUrl viewSystemUrl(SystemContext* systemContext) const;
    Q_INVOKABLE QUrl cloudLinkUrl(bool withReferral) const;

private:
    QUrl makeUrl(const QString& path = QString(), bool withReferral = true) const;

public:
    utils::SystemUri::ReferralSource m_source;
    utils::SystemUri::ReferralContext m_context;
};

} // namespace nx::vms::client::core
