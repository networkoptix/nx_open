// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_settings_adaptor.h"

#include <mobile_client/mobile_client_settings.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

using nx::vms::client::core::appContext;

namespace nx {
namespace client {
namespace mobile {

QmlSettingsAdaptor::QmlSettingsAdaptor(QObject* parent):
    QObject(parent)
{
    connect(qnSettings, &QnMobileClientSettings::valueChanged, this,
        [this](int id)
        {
            switch (id)
            {
                case QnMobileClientSettings::LiveVideoPreviews:
                    emit liveVideoPreviewsChanged();
                    break;

                case QnMobileClientSettings::LastUsedQuality:
                    emit lastUsedQualityChanged();
                    break;

                case QnMobileClientSettings::SavePasswords:
                    emit savePasswordsChanged();
                    break;

                case QnMobileClientSettings::HolePunchingOption:
                    emit enableHolePunchingChanged();
                    break;

                case QnMobileClientSettings::ForceTrafficLogging:
                    emit forceTrafficLoggingChanged();
                    break;

                case QnMobileClientSettings::CustomCloudHost:
                    emit customCloudHostChanged();
                    break;

                case QnMobileClientSettings::IgnoreCustomization:
                    emit ignoreCustomizationChanged();
                    break;

                case QnMobileClientSettings::UseDownloadVideoFeature:
                    emit useVideoDownloadFeatureChanged();
                    break;

                default:
                    break;
            }
        });

    connect(appContext()->coreSettings(), &vms::client::core::Settings::changed, this,
        [this](const auto property)
        {
            if (!property)
                return;

            if (property->name == appContext()->coreSettings()->locale.name)
                emit localeChanged();
        });
}

bool QmlSettingsAdaptor::liveVideoPreviews() const
{
    return qnSettings->liveVideoPreviews();
}

void QmlSettingsAdaptor::setLiveVideoPreviews(bool enable)
{
    qnSettings->setLiveVideoPreviews(enable);
}

int QmlSettingsAdaptor::lastUsedQuality() const
{
    return qnSettings->lastUsedQuality();
}

void QmlSettingsAdaptor::setLastUsedQuality(int quality)
{
    qnSettings->setLastUsedQuality(quality);
}

bool QmlSettingsAdaptor::savePasswords() const
{
    return qnSettings->savePasswords();
}

void QmlSettingsAdaptor::setSavePasswords(bool value)
{
    qnSettings->setSavePasswords(value);
}

QmlSettingsAdaptor::ValidationLevel QmlSettingsAdaptor::certificateValidationLevel() const
{
    return appContext()->coreSettings()->certificateValidationLevel();
}

void QmlSettingsAdaptor::setCertificateValidationLevel(ValidationLevel value)
{
    if (appContext()->coreSettings()->certificateValidationLevel() == value)
        return;

    appContext()->coreSettings()->certificateValidationLevel = value;

    emit certificateValidationLevelChanged();
}

bool QmlSettingsAdaptor::enableHardwareDecoding() const
{
    return appContext()->coreSettings()->enableHardwareDecoding();
}

void QmlSettingsAdaptor::setEnableHardwareDecoding(bool value)
{
    if (appContext()->coreSettings()->enableHardwareDecoding() == value)
        return;

    appContext()->coreSettings()->enableHardwareDecoding = value;
    emit enableHardwareDecodingChanged();
}

bool QmlSettingsAdaptor::enableHolePunching() const
{
    return qnSettings->enableHolePunching();
}

void QmlSettingsAdaptor::setEnableHolePunching(bool value)
{
    qnSettings->setEnableHolePunching(value);
    qnSettings->save();
}

bool QmlSettingsAdaptor::forceTrafficLogging() const
{
    return qnSettings->forceTrafficLogging();
}

void QmlSettingsAdaptor::setForceTrafficLogging(bool value)
{
    qnSettings->setForceTrafficLogging(value);
    qnSettings->save();
}

QString QmlSettingsAdaptor::customCloudHost() const
{
    return qnSettings->customCloudHost();
}

void QmlSettingsAdaptor::setCustomCloudHost(const QString& value)
{
    qnSettings->setCustomCloudHost(value);
    qnSettings->save();
}

bool QmlSettingsAdaptor::ignoreCustomization() const
{
    return qnSettings->ignoreCustomization();
}

void QmlSettingsAdaptor::setIgnoreCustomization(bool value)
{
    qnSettings->setIgnoreCustomization(value);
    qnSettings->save();
}

QString QmlSettingsAdaptor::locale() const
{
    return appContext()->coreSettings()->locale();
}

void QmlSettingsAdaptor::setLocale(const QString& value)
{
    if (locale() == value)
        return;

    appContext()->coreSettings()->locale = value;

    emit localeChanged();
}

bool QmlSettingsAdaptor::useVideoDownloadFeature() const
{
    return qnSettings->useDownloadVideoFeature();
}

void QmlSettingsAdaptor::setUseVideoDownloadFeature(bool value)
{
    qnSettings->setUseDownloadVideoFeature(value);
    qnSettings->save();
}

} // namespace mobile
} // namespace client
} // namespace nx
