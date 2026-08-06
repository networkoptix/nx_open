// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_promo_backend.h"

#include <QtGui/QDesktopServices>
#include <QtQml/QtQml>

#include <nx/branding.h>
#include <nx/utils/url.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/settings/local_settings.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::mobile {

SaasPromoBackend::SaasPromoBackend(QObject* parent): QObject(parent)
{
    connect(&appContext()->localSettings()->showSaasPromo,
        &LocalSettings::BaseProperty::changed,
        this,
        &SaasPromoBackend::updateAvailability);
}

void SaasPromoBackend::setSystemContext(core::SystemContext* context)
{
    if (m_context == context)
        return;

    m_contextConnections.reset();
    m_context = context;

    if (context)
    {
        m_contextConnections << connect(context,
            &core::SystemContext::userChanged,
            this,
            &SaasPromoBackend::updateAvailability);

        m_contextConnections << connect(context->accessController(),
            &core::AccessController::globalPermissionsChanged,
            this,
            &SaasPromoBackend::updateAvailability);

        m_contextConnections << connect(context->globalSettings(),
            &common::SystemSettings::cloudSettingsChanged,
            this,
            &SaasPromoBackend::updateAvailability);

        m_contextConnections << connect(context->globalSettings(),
            &common::SystemSettings::organizationIdChanged,
            this,
            &SaasPromoBackend::updateAvailability);
    }

    updateAvailability();
    emit systemContextChanged();
}

void SaasPromoBackend::updateAvailability()
{
    const bool available = [this]()
    {
        if (!m_context)
            return false;

        const auto settings = m_context->globalSettings();

        return m_context->accessController()->hasPowerUserPermissions()
            && !settings->cloudSystemId().isEmpty() && settings->organizationId().isNull()
            && !nx::branding::saasPromoUrl().isEmpty()
            && appContext()->localSettings()->showSaasPromo();
    }();

    if (m_available == available)
        return;

    m_available = available;
    emit availableChanged();
}

void SaasPromoBackend::dismiss()
{
    appContext()->localSettings()->showSaasPromo = false;
}

void SaasPromoBackend::openPromoUrl()
{
    const auto url = nx::Url::fromUserInput(nx::branding::saasPromoUrl());
    if (url.isValid())
        QDesktopServices::openUrl(url.toQUrl());
}

void SaasPromoBackend::registerQmlType()
{
    qmlRegisterType<SaasPromoBackend>("nx.vms.client.mobile", 1, 0, "SaasPromoBackend");
}

} // namespace nx::vms::client::mobile
