#ifndef __AVI_ARCHIVE_CUSTOM_DATA_H
#define __AVI_ARCHIVE_CUSTOM_DATA_H

#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>

/** Struct for adding custom fields to an exported video file. */
struct QnAviArchiveCustomData {
    qreal overridenAr;

    QnAviArchiveCustomData();
};

QN_FUSION_DECLARE_FUNCTIONS(QnAviArchiveCustomData, (json)(metatype))

#endif //__AVI_ARCHIVE_CUSTOM_DATA_H