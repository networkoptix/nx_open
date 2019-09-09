#pragma once

#include <core/resource/client_core_camera.h>

#include "client_resource_fwd.h"

class QnArchiveStreamReader;

class QnClientCameraResource: public nx::vms::client::core::Camera
{
    Q_OBJECT
    using base_type = nx::vms::client::core::Camera;

public:
    explicit QnClientCameraResource(const QnUuid& resourceTypeId);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(
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

signals:
    void dataDropped(QnArchiveStreamReader* reader);
protected slots:
    virtual void resetCachedValues() override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
private:
    Qn::ResourceFlags calculateFlags() const;
private:
    mutable std::atomic<Qn::ResourceFlags> m_cachedFlags{};
};
