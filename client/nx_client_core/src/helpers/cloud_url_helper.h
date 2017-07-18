#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/vms/utils/system_uri.h>

class QnCloudUrlHelper: public QObject
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
    Q_INVOKABLE QUrl createAccountUrl() const;
    Q_INVOKABLE QUrl restorePasswordUrl() const;
    Q_INVOKABLE QUrl faqUrl() const;
    Q_INVOKABLE QUrl viewSystemUrl() const;

    Q_INVOKABLE QUrl makeUrl(const QString& path = QString(), bool auth = true) const;

public:
    nx::vms::utils::SystemUri::ReferralSource m_source;
    nx::vms::utils::SystemUri::ReferralContext m_context;
};
