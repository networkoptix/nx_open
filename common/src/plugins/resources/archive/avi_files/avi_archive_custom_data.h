#ifndef __AVI_ARCHIVE_CUSTOM_DATA_H
#define __AVI_ARCHIVE_CUSTOM_DATA_H

#include <QtCore/QMetaType>

#include <utils/common/json_fwd.h>

/** Struct for adding custom fields to an exported video file. */
struct QnAviArchiveCustomData {
    qreal overridenAr;

    QnAviArchiveCustomData();
};

Q_DECLARE_METATYPE(QnAviArchiveCustomData)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnAviArchiveCustomData)

#endif //__AVI_ARCHIVE_CUSTOM_DATA_H