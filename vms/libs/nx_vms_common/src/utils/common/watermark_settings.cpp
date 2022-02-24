// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "watermark_settings.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/compare.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnWatermarkSettings, (json), QnWatermarkSettings_Fields)

bool QnWatermarkSettings::operator==(const QnWatermarkSettings& other) const
{
    return nx::reflect::equals(*this, other);
}
