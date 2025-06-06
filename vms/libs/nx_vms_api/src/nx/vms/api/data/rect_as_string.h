// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/json/qt_geometry_reflect.h>

namespace nx::vms::api {

class NX_VMS_API RectAsString: public QRectF
{
public:
    using QRectF::QRectF;
    RectAsString(const QRectF& crop): QRectF(crop) {}
    std::string toString() const;
};
QN_FUSION_DECLARE_FUNCTIONS(RectAsString, (json), NX_VMS_API)
bool NX_VMS_API fromString(const std::string& str, RectAsString* target);
NX_REFLECTION_TAG_TYPE(RectAsString, useStringConversionForSerialization)

} // namespace nx::vms::api
