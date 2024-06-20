// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QProperty>
#include <QtCore/QVariant>

#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class ApiIntegrationRequestsModel:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    static constexpr auto kRefreshInterval = std::chrono::seconds(3);

public:
    Q_PROPERTY(QVariant requests READ requests NOTIFY requestsChanged)
    Q_PROPERTY(bool isActive MEMBER m_isActive NOTIFY isActiveChanged)

    ApiIntegrationRequestsModel(SystemContext* systemContext, QObject* parent = nullptr);

    QVariant requests() const { return m_requests; }
    void setRequests(const QVariant& requests);
    void refresh();
    Q_INVOKABLE void reject(const QString& id);
    Q_INVOKABLE void approve(const QString& id);

signals:
    void requestsChanged();
    void isActiveChanged();

private:
    QProperty<bool> m_isActive{false};
    QVariant m_requests;
    bool m_isNewRequestsEnabled = false;
};

} // namespace nx::vms::client::desktop
