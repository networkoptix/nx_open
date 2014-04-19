#include "avi_archive_custom_data.h"

#include <utils/common/model_functions.h>
#include <utils/serialization/json_functions.h>
#include <utils/fusion/fusion_adaptor.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAviArchiveCustomData,      (json),    (overridenAr))

QnAviArchiveCustomData::QnAviArchiveCustomData():
    overridenAr(0.0) {}