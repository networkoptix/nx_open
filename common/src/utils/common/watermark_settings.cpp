#include "watermark_settings.h"

#include <QtCore/QByteArray>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnWatermarkSettings, (json)(eq), QnWatermarkSettings_Fields)
