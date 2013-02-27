#ifndef QN_STORAGE_SPACE_DATA_H
#define QN_STORAGE_SPACE_DATA_H

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QMetaType>

#include <utils/common/json.h>

struct QnStorageSpaceData {
	QString path;
	int storageId;
	qint64 totalSpace;
	qint64 freeSpace;
    qint64 reservedSpace;
    bool isWritable;
    bool isUsedForWriting;
};

QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(QnStorageSpaceData, (path)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isWritable)(isUsedForWriting), inline)

typedef QList<QnStorageSpaceData> QnStorageSpaceDataList;

Q_DECLARE_METATYPE(QnStorageSpaceData);
Q_DECLARE_METATYPE(QnStorageSpaceDataList);

#endif // QN_STORAGE_SPACE_DATA_H
