// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <string_view>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

class QColor;
class QRegion;

NX_VMS_COMMON_API std::string toString(const QColor& value);
NX_VMS_COMMON_API bool fromString(const std::string_view& str, QColor* value);

NX_REFLECTION_TAG_TYPE(QColor, useStringConversionForSerialization)

QN_FUSION_DECLARE_FUNCTIONS(QRegion, (json), NX_VMS_COMMON_API)
