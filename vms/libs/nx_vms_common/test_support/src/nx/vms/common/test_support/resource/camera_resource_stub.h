// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_resource.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

namespace nx {

class NX_VMS_COMMON_TEST_SUPPORT_API CameraResourceStub: public QnVirtualCameraResource
{
    using base_type = QnVirtualCameraResource;
    using StreamIndex = nx::vms::api::StreamIndex;

public:
    CameraResourceStub(Qn::LicenseType licenseType = Qn::LC_Professional);
    CameraResourceStub(const QSize& primaryResolution, const QSize& secondaryResolution = QSize(),
        Qn::LicenseType licenseType = Qn::LC_Professional);
    virtual ~CameraResourceStub() override;

    virtual bool hasDualStreamingInternal() const override;
    void setHasDualStreaming(bool value);

    void markCameraAsNvr();
    void markCameraAsVMax();

    void setLicenseType(Qn::LicenseType licenseType);

    virtual bool setProperty(
        const QString& key,
        const QString& value,
        bool markDirty = true) override;

    void setStreamResolution(StreamIndex index, const QSize& resolution);

    /** Emulate camera ability to produce analytics objects. */
    void setAnalyticsObjectsEnabled(
        bool value = true,
        const nx::Uuid& engineId = nx::Uuid::createUuid());

    virtual AnalyticsEntitiesByEngine supportedObjectTypes(
        bool filterByEngines = true) const override;
    void setSupportedObjectTypes(const QMap<nx::Uuid, std::set<QString>>& supportedObjectTypes);

    virtual AnalyticsEntitiesByEngine supportedEventTypes() const override;
    void setSupportedEventTypes(const QMap<nx::Uuid, std::set<QString>>& eventTypesByEngine);

    virtual UuidSet enabledAnalyticsEngines() const override;
    void setEnabledAnalyticsEngines(UuidSet engines);

    virtual nx::vms::common::AnalyticsEngineResourceList
        compatibleAnalyticsEngineResources() const override;
    void setCompatibleAnalyticsEngineResources(
        nx::vms::common::AnalyticsEngineResourceList engines);

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual Qn::LicenseType calculateLicenseType() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

using CameraResourceStubPtr = QnSharedResourcePointer<CameraResourceStub>;
using StubCameraResourceList = QnSharedResourcePointerList<CameraResourceStub>;

} // namespace nx
