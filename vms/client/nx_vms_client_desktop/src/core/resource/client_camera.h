// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/core/resource/camera.h>

#include "client_resource_fwd.h"

class QnArchiveStreamReader;

class NX_VMS_CLIENT_DESKTOP_API QnClientCameraResource:
    public nx::vms::client::core::Camera
{
    Q_OBJECT
    using base_type = nx::vms::client::core::Camera;

public:
    explicit QnClientCameraResource(const QnUuid& resourceTypeId);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) override;
    virtual AudioLayoutConstPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) const override;

    virtual Qn::ResourceFlags flags() const override;

    static void setAuthToCameraGroup(
        const QnVirtualCameraResourcePtr& camera,
        const QAuthenticator& authenticator);

    static QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role);

    /**
     * Debug log representation. Used by toString(const T*).
     */
    virtual QString idForToStringFromPtr() const override;

    bool isIntercom() const;

    /**
    * Whether client should automatically send PTZ Stop command when camera loses focus.
    * Enabled by default, can be disabled by setting a special resource property.
    */
    bool autoSendPtzStopCommand() const;
    void setAutoSendPtzStopCommand(bool value);

signals:
    void dataDropped();

protected slots:
    virtual void resetCachedValues() override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
private:
    Qn::ResourceFlags calculateFlags() const;
private:
    mutable std::atomic<Qn::ResourceFlags> m_cachedFlags{};
    nx::utils::CachedValue<bool> m_isIntercom;
};
