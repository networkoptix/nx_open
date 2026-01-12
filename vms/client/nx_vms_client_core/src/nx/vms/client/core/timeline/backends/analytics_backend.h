// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/db/analytics_db_types.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

#include "abstract_backend.h"

namespace nx::vms::client::core {
namespace timeline {

using ObjectTrackList = nx::analytics::db::LookupResult;

template<>
struct DataListTraits<ObjectTrackList>
{
    static std::chrono::milliseconds timestamp(const auto& item)
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(microseconds(item.firstAppearanceTimeUs));
    }
};

class NX_VMS_CLIENT_CORE_API AnalyticsBackend: public AbstractBackend<ObjectTrackList>
{
public:
    explicit AnalyticsBackend(const QnResourcePtr& resource);
    virtual ~AnalyticsBackend() override;

    virtual QnResourcePtr resource() const override;
    virtual QFuture<ObjectTrackList> load(const QnTimePeriod& period, int limit) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::core
