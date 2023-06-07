// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

class RectAsString: public QRectF
{
public:
    RectAsString() = default;
    RectAsString(const QRectF& crop): QRectF(crop) {}
};

bool deserialize(QnJsonContext* context, const QJsonValue& value, RectAsString* target);

} // namespace nx::vms::api
