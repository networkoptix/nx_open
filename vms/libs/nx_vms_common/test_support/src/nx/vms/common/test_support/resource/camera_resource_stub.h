// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
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
        const QnUuid& engineId = QnUuid::createUuid());

    virtual QSet<QnUuid> enabledAnalyticsEngines() const override;

    virtual std::map<QnUuid, std::set<QString>> supportedObjectTypes(
        bool filterByEngines = true) const override;
    void setSupportedObjectTypes(const QMap<QnUuid, std::set<QString>>& supportedObjectTypes);

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
