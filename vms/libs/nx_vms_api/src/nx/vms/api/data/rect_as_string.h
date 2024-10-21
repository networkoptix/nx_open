// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

class RectAsString: public QRectF
{
public:
    using QRectF::QRectF;
    RectAsString(const QRectF& crop): QRectF(crop) {}
};
QN_FUSION_DECLARE_FUNCTIONS(RectAsString, (json), NX_VMS_API)
NX_REFLECTION_TAG_TYPE(RectAsString, useStringConversionForSerialization)
bool NX_VMS_API fromString(const std::string& str, RectAsString* target);

} // namespace nx::vms::api
