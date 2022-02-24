// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "immediate_image_result.h"

namespace nx::vms::client::core {

ImmediateImageResult::ImmediateImageResult(const QImage& image, QObject* parent):
    base_type(parent)
{
    setImage(image);
}

} // namespace nx::vms::client::core
