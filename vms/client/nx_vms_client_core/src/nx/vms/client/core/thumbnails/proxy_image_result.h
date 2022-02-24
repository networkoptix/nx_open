// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "async_image_result.h"

#include <memory>

namespace nx::vms::client::core {

class ProxyImageResult: public AsyncImageResult
{
    Q_OBJECT
    using base_type = AsyncImageResult;

public:
    explicit ProxyImageResult(AsyncImageResult* source, QObject* parent = nullptr);
    explicit ProxyImageResult(std::shared_ptr<AsyncImageResult> source, QObject* parent = nullptr);
};

} // namespace nx::vms::client::core
