#ifndef QN_STORAGE_SPACE_REPLY_H
#define QN_STORAGE_SPACE_REPLY_H

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

struct QnStorageSpaceReply {
    QList<QnStorageSpaceData> storages;
    QList<QString> storagePlugins;
};

QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(QnStorageSpaceData, (path)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isWritable)(isUsedForWriting), inline)
QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(QnStorageSpaceReply, (storages)(storagePlugins), inline)

Q_DECLARE_METATYPE(QnStorageSpaceReply);

#endif // QN_STORAGE_SPACE_DATA_H
