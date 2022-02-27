// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "async_image_result.h"

namespace nx::vms::client::core {

class ImmediateImageResult: public AsyncImageResult
{
    Q_OBJECT
    using base_type = AsyncImageResult;

public:
    explicit ImmediateImageResult(const QImage& image, QObject* parent = nullptr);
};

} // namespace nx::vms::client::core
