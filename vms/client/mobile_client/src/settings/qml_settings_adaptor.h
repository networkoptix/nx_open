// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/core/network/server_certificate_validation_level.h>

namespace nx {
namespace client {
namespace mobile {

class QmlSettingsAdaptor: public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool liveVideoPreviews
        READ liveVideoPreviews WRITE setLiveVideoPreviews
        NOTIFY liveVideoPreviewsChanged)
    Q_PROPERTY(int lastUsedQuality
        READ lastUsedQuality WRITE setLastUsedQuality
        NOTIFY lastUsedQualityChanged)
    Q_PROPERTY(bool savePasswords
        READ savePasswords WRITE setSavePasswords
        NOTIFY savePasswordsChanged)
    Q_PROPERTY(nx::vms::client::core::network::server_certificate::ValidationLevel
        certificateValidationLevel
        READ certificateValidationLevel WRITE setCertificateValidationLevel
        NOTIFY certificateValidationLevelChanged)
    Q_PROPERTY(bool enableHardwareDecoding
        READ enableHardwareDecoding
        WRITE setEnableHardwareDecoding
        NOTIFY enableHardwareDecodingChanged)
    Q_PROPERTY(bool forceTrafficLogging
        READ forceTrafficLogging
        WRITE setForceTrafficLogging
        NOTIFY forceTrafficLoggingChanged)
    Q_PROPERTY(QString customCloudHost
        READ customCloudHost
        WRITE setCustomCloudHost
        NOTIFY customCloudHostChanged)
    Q_PROPERTY(bool ignoreCustomization
        READ ignoreCustomization
        WRITE setIgnoreCustomization
        NOTIFY ignoreCustomizationChanged)

    Q_PROPERTY(QString locale
        READ locale
        WRITE setLocale
        NOTIFY localeChanged)

    // Beta features.
    Q_PROPERTY(bool enableHolePunching
        READ enableHolePunching
        WRITE setEnableHolePunching
        NOTIFY enableHolePunchingChanged)

    Q_PROPERTY(bool useVideoDownloadFeature
        WRITE setUseVideoDownloadFeature
        READ useVideoDownloadFeature
        NOTIFY useVideoDownloadFeatureChanged)

    Q_PROPERTY(bool useMaxHardwareDecodersCount
        WRITE setUseMaxHardwareDecodersCount
        READ useMaxHardwareDecodersCount
        NOTIFY useMaxHardwareDecodersCountChanged)

    Q_PROPERTY(bool enableSoftwareDecoderFallback
        READ enableSoftwareDecoderFallback
        WRITE setEnableSoftwareDecoderFallback
        NOTIFY enableSoftwareDecoderFallbackChanged)

public:
    explicit QmlSettingsAdaptor(QObject* parent = nullptr);

    bool liveVideoPreviews() const;
    void setLiveVideoPreviews(bool enable);

    int lastUsedQuality() const;
    void setLastUsedQuality(int quality);

    bool savePasswords() const;
    void setSavePasswords(bool value);

    using ValidationLevel = nx::vms::client::core::network::server_certificate::ValidationLevel;
    ValidationLevel certificateValidationLevel() const;
    void setCertificateValidationLevel(ValidationLevel value);

    bool enableHardwareDecoding() const;
    void setEnableHardwareDecoding(bool value);

    bool enableHolePunching() const;
    void setEnableHolePunching(bool value);

    bool forceTrafficLogging() const;
    void setForceTrafficLogging(bool value);

    QString customCloudHost() const;
    void setCustomCloudHost(const QString& value);

    bool ignoreCustomization() const;
    void setIgnoreCustomization(bool value);

    QString locale() const;
    void setLocale(const QString& value);

    bool useVideoDownloadFeature() const;
    void setUseVideoDownloadFeature(bool value);

    bool useMaxHardwareDecodersCount() const;
    void setUseMaxHardwareDecodersCount(bool value);

    bool enableSoftwareDecoderFallback() const;
    void setEnableSoftwareDecoderFallback(bool value);

signals:
    void liveVideoPreviewsChanged();
    void lastUsedQualityChanged();
    void savePasswordsChanged();
    void certificateValidationLevelChanged();
    void enableHardwareDecodingChanged();
    void enableHolePunchingChanged();
    void forceTrafficLoggingChanged();
    void customCloudHostChanged();
    void ignoreCustomizationChanged();
    void localeChanged();
    void useVideoDownloadFeatureChanged();
    void useMaxHardwareDecodersCountChanged();
    void enableSoftwareDecoderFallbackChanged();
};

} // namespace mobile
} // namespace client
} // namespace nx
