// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("core/resource/resource.h")
Q_MOC_INCLUDE("nx/vms/client/desktop/window_context.h")

namespace nx::vms::client::desktop {

class WindowContext;

class ResourceStatusHelper: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::desktop::WindowContext* context
        READ context WRITE setContext NOTIFY contextChanged)
    Q_PROPERTY(QnResource* resource READ resource WRITE setResource NOTIFY resourceChanged)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(ResourceType resourceType READ resourceType NOTIFY resourceChanged)
    Q_PROPERTY(LicenseUsage licenseUsage READ licenseUsage NOTIFY licenseUsageChanged)
    Q_PROPERTY(int defaultPasswordCameras
        READ defaultPasswordCameras NOTIFY defaultPasswordCamerasChanged)

    using base_type = QObject;

public:
    explicit ResourceStatusHelper(QObject* parent = nullptr);
    virtual ~ResourceStatusHelper() override;

    WindowContext* context() const;
    void setContext(WindowContext* value);

    QnResource* resource() const;
    void setResource(QnResource* value);

    enum class StatusFlag
    {
        offline = 0x0001,
        unauthorized = 0x0002,
        oldFirmware = 0x0004,
        noLiveStream = 0x0008,
        defaultPassword = 0x0010,
        canViewLive = 0x0020,
        canViewArchive = 0x0040,
        canEditSettings = 0x0080,
        canInvokeDiagnostics = 0x0100,
        canChangePasswords = 0x0200,
        dts = 0x0400,
        videowallLicenseRequired = 0x0800,
        noVideoData = 0x1000,
        mismatchedCertificate = 0x2000,
        accessDenied = 0x4000,
    };
    Q_ENUM(StatusFlag)
    Q_DECLARE_FLAGS(StatusFlags, StatusFlag)

    int status() const { return statusFlags(); }
    StatusFlags statusFlags() const;

    enum class ResourceType
    {
        none, //< Set when resource is null.
        camera,
        ioModule,
        localMedia,
        other //< Can be split into particular types if needed in the future.
    };
    Q_ENUM(ResourceType)
    ResourceType resourceType() const;

    int defaultPasswordCameras() const;

    enum class LicenseUsage
    {
        notUsed,
        overflow,
        used
    };
    Q_ENUM(LicenseUsage)
    LicenseUsage licenseUsage() const;

    enum class ActionType
    {
        diagnostics,
        enableLicense,
        moreLicenses,
        settings,
        setPassword,
        setPasswords
    };
    Q_ENUM(ActionType)

    Q_INVOKABLE void executeAction(ActionType type);

    static void registerQmlType();

signals:
    void contextChanged();
    void resourceChanged();
    void statusChanged();
    void defaultPasswordCamerasChanged();
    void licenseUsageChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
