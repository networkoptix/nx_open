// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <common/common_globals.h>
#include <nx/utils/impl_ptr.h>

#include "abstract_content_watcher.h"

Q_MOC_INCLUDE("nx/vms/client/core/media/chunk_provider.h")

namespace nx::vms::client::core { class ChunkProvider; }

namespace nx::vms::client::mobile {
namespace timeline {

class BookmarkContentWatcher: public AbstractContentWatcher
{
    Q_OBJECT

public:
    explicit BookmarkContentWatcher(core::ChunkProvider* resourceHolder,
        std::chrono::seconds updateInterval = std::chrono::seconds{30});

    virtual ~BookmarkContentWatcher() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
