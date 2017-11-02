#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/vms/utils/system_uri.h>
#include <nx/utils/url.h>

class QnCloudUrlHelper: public QObject
{
    Q_OBJECT

public:
    QnCloudUrlHelper(
        nx::vms::utils::SystemUri::ReferralSource source,
        nx::vms::utils::SystemUri::ReferralContext context,
        QObject* parent = nullptr);

    Q_INVOKABLE nx::utils::Url mainUrl() const;
    Q_INVOKABLE nx::utils::Url aboutUrl() const;
    Q_INVOKABLE nx::utils::Url accountManagementUrl() const;
    Q_INVOKABLE nx::utils::Url createAccountUrl() const;
    Q_INVOKABLE nx::utils::Url restorePasswordUrl() const;
    Q_INVOKABLE nx::utils::Url faqUrl() const;
    Q_INVOKABLE nx::utils::Url viewSystemUrl() const;

    Q_INVOKABLE nx::utils::Url makeUrl(const QString& path = QString(), bool auth = true) const;

public:
    nx::vms::utils::SystemUri::ReferralSource m_source;
    nx::vms::utils::SystemUri::ReferralContext m_context;
};
