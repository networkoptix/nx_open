#include "avi_archive_custom_data.h"

#ifdef ENABLE_ARCHIVE

#include <utils/common/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnAviArchiveCustomData), (json), _Fields)

QnAviArchiveCustomData::QnAviArchiveCustomData():
    overridenAr(0.0) 
{}

#endif // ENABLE_ARCHIVE
