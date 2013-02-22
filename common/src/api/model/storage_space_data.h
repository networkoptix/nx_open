#ifndef QN_STORAGE_SPACE_DATA_H
#define QN_STORAGE_SPACE_DATA_H

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QMetaType>

struct QnStorageSpaceData {
	QString path;
	int storageId;
	qint64 totalSpace;
	qint64 freeSpace;
};

void serialize(const QnStorageSpaceData &value, QVariant *target);
bool deserialize(const QVariant &value, QnStorageSpaceData *target);

Q_DECLARE_METATYPE(QnStorageSpaceData);

#endif // QN_STORAGE_SPACE_DATA_H
