// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_resource.h>

#include "resource_fwd.h"

class QAuthenticator;
class QnAbstractStreamDataProvider;

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CameraResource: public QnVirtualCameraResource
{
    Q_OBJECT
    using base_type = QnVirtualCameraResource;

public:
    explicit CameraResource(const nx::Uuid& resourceTypeId);

    /**
     * @return User-defined camera name if it is present, default name otherwise.
     */
    virtual QString getName() const override;

    /**
     * Set user-defined camera name.
     */
    virtual void setName(const QString& name) override;

    virtual nx::vms::api::ResourceStatus getStatus() const override;

    virtual Qn::ResourceFlags flags() const override;
    virtual void setParentId(const nx::Uuid& parent) override;

    /** Improved check which includes current user permissions validation. */
    virtual bool hasAudio() const override;

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) override;
    virtual AudioLayoutConstPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) const override;

    QnAbstractStreamDataProvider* createDataProvider(Qn::ConnectionRole role);

    bool isPtzSupported() const;
    bool isPtzRedirected() const;
    CameraResourcePtr ptzRedirectedTo() const;

    /**
     * Whether client should automatically send PTZ Stop command when camera loses focus.
     * Enabled by default, can be disabled by setting a special resource property.
     */
    bool autoSendPtzStopCommand() const;
    void setAutoSendPtzStopCommand(bool value);

    static void setAuthToCameraGroup(
        const QnVirtualCameraResourcePtr& camera,
        const QAuthenticator& authenticator);

    /**
     * Debug log representation. Used by toString(const T*).
     */
    virtual QString idForToStringFromPtr() const override;

signals:
    void dataDropped();
    void footageAdded();

protected:
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;
};

} // namespace nx::vms::client::core
