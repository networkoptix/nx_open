// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

#include "abstract_backend.h"

namespace nx::vms::client::core {
namespace timeline {

template<>
struct DataListTraits<QnCameraBookmarkList>
{
    static std::chrono::milliseconds timestamp(const auto& item) { return item.startTimeMs; }
};

class NX_VMS_CLIENT_CORE_API BookmarksBackend: public AbstractBackend<QnCameraBookmarkList>
{
public:
    explicit BookmarksBackend(const QnResourcePtr& resource);
    virtual ~BookmarksBackend() override;

    virtual QnResourcePtr resource() const override;
    virtual QFuture<QnCameraBookmarkList> load(const QnTimePeriod& period, int limit) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::core
